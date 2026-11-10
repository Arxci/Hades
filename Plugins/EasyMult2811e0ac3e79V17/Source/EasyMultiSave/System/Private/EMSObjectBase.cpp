//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSObjectBase.h"
#include "EMSPaths.h"
#include "EMSCustomSaveGame.h"
#include "EMSInfoSaveGame.h"
#include "EMSCore.h"
#include "EMSDebug.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ArchiveSaveCompressedProxy.h"
#include "Serialization/ArchiveLoadCompressedProxy.h"
#include "GameFramework/SaveGame.h"
#include "SaveGameSystem.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "HAL/FileManager.h"
#include "PlatformFeatures.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"

/**
Init
**/

UEMSObjectBase::UEMSObjectBase()
{
}

void UEMSObjectBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentSaveGameName = ValidateSaveName(CurrentSaveGameName);
	CurrentSaveUserName = ValidateSaveName(CurrentSaveUserName);

	//A config written before the reserved-name rule existed. Clear it so the getter fallback applies.
	if (FSavePaths::IsSlotNameReserved(CurrentSaveGameName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Reserved Slot name in config was discarded: %s"), *CurrentSaveGameName);
		CurrentSaveGameName.Empty();
	}

	const FString VersionNum = VERSION_STRINGIFY(EMS_VERSION_NUMBER);
	UE_LOG(LogEasyMultiSave, Log, TEXT("Easy Multi Save Initialized | Version: %s | Current Save Game Slot: %s"),
		*VersionNum,
		*GetCurrentSaveGameName());
}

void UEMSObjectBase::Deinitialize()
{
	Super::Deinitialize();

	CommitSaveReportOnShutdown();
}

UWorld* UEMSObjectBase::GetWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

/**
Save Object Creation
**/

template <class TSaveGame>
TSaveGame* UEMSObjectBase::CreateNewSaveObject(const FString& FullSavePath, const FSoftClassPath& InClassName)
{
	if (FullSavePath.IsEmpty())
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Save path is empty"));
		return nullptr;
	}

	//Try to load the class from the provided class name
	const FSoftClassPath LocalClassName = InClassName;
	TSubclassOf<TSaveGame> Class = LocalClassName.TryLoadClass<TSaveGame>();

	if (!Class)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Invalid Save Game Object Class: %s"), *LocalClassName.ToString());
		return nullptr;
	}

	//Load save game object 
	USaveGame* SaveGame = NewObject<USaveGame>(this, Class);
	if (!SaveGame)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Failed to load Save Game Object: %s"), *FullSavePath);
		return nullptr;
	}

	TSaveGame* SaveGameObject = Cast<TSaveGame>(SaveGame);
	if (!SaveGameObject)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Invalid Save Game Object: %s"), *FullSavePath);
		return nullptr;
	}

	/*
	Load the archive, warn if corrupt
	@TODO This can in theory overwrite valid data with corrupt data, but it almost will never happen
	*/
	if (DoesFileExist(FullSavePath))
	{
		const FLoadArchiveContext Context = FLoadArchiveContext(FullSavePath, EDataLoadType::DATA_Object, FSaveObjects(SaveGameObject));
		if (!LoadBinaryArchive(Context))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Save Game Object data is invalid. Please overwrite the file with proper data: %s"), *FullSavePath);
			return SaveGameObject;   
		}
	}

	return SaveGameObject;
}

/**
Custom Save Objects
**/

static FName MakeCachedCustomSaveName(const FString& CustomSaveName, const FString& SlotName)
{
	return EMS::ToCanonicalFName(CustomSaveName + EMS::CustomSaveTag + SlotName);
}

UEMSCustomSaveGame* UEMSObjectBase::GetCustomSave(const TSubclassOf<UEMSCustomSaveGame>& SaveGameClass,
	const FString& InSlotName, const FString& InFileName, const bool bCacheResult)
{
	if (!SaveGameClass || SaveGameClass->HasAnyClassFlags(CLASS_Abstract))
	{
		if (SaveGameClass)
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Custom Save Class '%s' is abstract and cannot be used."), *SaveGameClass->GetName());
		}

		return nullptr;
	}

	const UEMSCustomSaveGame* CustomClass = Cast<UEMSCustomSaveGame>(SaveGameClass->GetDefaultObject());
	if (!CustomClass)
	{
		return nullptr;
	}

	//Resolve the final save name (custom override or fallback class name. Never empty)
	const FString ResolvedSaveName = [InFileName, CustomClass]() -> FString
	{
		if (EMS::StringEmpty(InFileName))
		{
			return EMS::StringEmpty(CustomClass->SaveGameName) ? CustomClass->GetName() : CustomClass->SaveGameName;
		}

		return InFileName;

	}();

	const bool bUseSlot = CustomClass->bUseSaveSlot;
	const bool bCustomSlotName = bUseSlot && !EMS::StringEmpty(InSlotName);
	const FString ActualSlot = bCustomSlotName ? InSlotName : GetCurrentSaveGameName();
	const FString SlotName = bUseSlot ? ValidateSaveName(ActualSlot) : FString();

	//Only a caller supplied Slot has to be checked, the Current Slot can never be reserved.
	if (bCustomSlotName && FSavePaths::IsSlotNameReserved(SlotName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Reserved Slot name rejected for Custom Save: %s"), *SlotName);
		return nullptr;
	}

	//Make sure the name is not invalid, regardless of bUseSlot
	const FString ValidCustomSaveName = ValidateSaveName(ResolvedSaveName);
	const FString CustomSaveName =
		IsFileNameReserved(ValidCustomSaveName) ?
		CustomClass->GetName() :
		ValidCustomSaveName;

	const FName CachedRefName = MakeCachedCustomSaveName(CustomSaveName, SlotName);

	if (UEMSCustomSaveGame* CachedObject = CachedCustomSaves.FindRef(CachedRefName))
	{
		if (bUseSlot) CachedObject->SlotName = SlotName;
		return CachedObject;
	}

	const FString SaveFile = CustomSaveFile(CustomSaveName, SlotName);
	const FSoftClassPath SaveClass = CustomClass->GetClass();

	//Lazy create
	if (UEMSCustomSaveGame* NewObject = CreateNewSaveObject<UEMSCustomSaveGame>(SaveFile, SaveClass))
	{
		NewObject->SaveGameName = CustomSaveName;
		NewObject->SlotName = SlotName;

		if (bCacheResult)
		{
			CachedCustomSaves.Add(CachedRefName, NewObject);
			UE_LOG(LogEasyMultiSave, Verbose, TEXT("Cached Custom Save added: %s"), *CachedRefName.ToString());
		}

		return NewObject;
	}

	UE_LOG(LogEasyMultiSave, Error, TEXT("Custom Save access failed: %s"), *CustomSaveName);

	return nullptr;
}

bool UEMSObjectBase::SaveCustom(UEMSCustomSaveGame* SaveGame)
{
	if (!IsValid(SaveGame))
	{
		return false;
	}

	//Names here are validated from the Getter
	const bool bUseSlot = SaveGame->bUseSaveSlot;
	const FString SlotName = bUseSlot ? SaveGame->SlotName : FString();

	const FString CustomSaveName = SaveGame->SaveGameName;
	if (EMS::StringEmpty(CustomSaveName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Save Custom with invalid SaveGameName. Aborting!"));
		return false;
	}

	if (SaveObject(*CustomSaveFile(CustomSaveName, SlotName), SaveGame))
	{
		if (bUseSlot)
		{
			SaveSlotInfoObject(SlotName);
			UE_LOG(LogEasyMultiSave, Log, TEXT("Custom Save Object saved: %s for Slot: %s"), *CustomSaveName, *SlotName);
		}
		else
		{
			UE_LOG(LogEasyMultiSave, Log, TEXT("Custom Save Object saved: %s"), *CustomSaveName);
		}

		return true;
	}

	UE_LOG(LogEasyMultiSave, Warning, TEXT("Failed to save Custom Save Object: %s"), *CustomSaveName);

	return false;
}

bool UEMSObjectBase::TransferCustom(UEMSCustomSaveGame* SaveGame, const FString& InTargetSlot)
{
	if (!IsValid(SaveGame) || !SaveGame->bUseSaveSlot)
	{
		return false;
	}

	const FString TargetSlot = ValidateSaveName(InTargetSlot);
	if (EMS::StringEmpty(TargetSlot) || FSavePaths::IsSlotNameReserved(TargetSlot))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Invalid Target Slot for Custom Save transfer: %s"), *InTargetSlot);
		return false;
	}

	const FString CustomSaveName = SaveGame->SaveGameName;
	const FString SourceSlot = SaveGame->SlotName;
	if (EMS::StringEmpty(CustomSaveName) || EMS::StringEmpty(SourceSlot))
	{
		return false;
	}

	//A Target that resolves to the Source is an ordinary Save, which needs no existing source file
	if (SourceSlot.Equals(TargetSlot, ESearchCase::IgnoreCase))
	{
		return SaveCustom(SaveGame);
	}

	//Nothing to transfer, would otherwise write a default object over the Target
	if (!DoesCustomSaveExist(SourceSlot, CustomSaveName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Custom Save transfer skipped, no source data: %s in Slot: %s"),
			*CustomSaveName, *SourceSlot);
		return false;
	}

	if (CheckSaveGameIntegrity(CustomSaveFile(CustomSaveName, SourceSlot)) == EIntegrityCheckResult::Invalid)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Custom Save transfer skipped, source data is invalid: %s in Slot: %s"),
			*CustomSaveName, *SourceSlot);
		return false;
	}

	const FString TargetFile = CustomSaveFile(CustomSaveName, TargetSlot);
	if (SaveObject(*TargetFile, SaveGame))
	{
		//The Target primary changed, so never retain a cached pre-transfer object.
		ClearCachedCustomSave(TargetSlot, CustomSaveName);

		//Same metadata semantics as Copy Custom Only and Cloud CustomSlot.
		if (!StampSlotInfo(TargetSlot, TEXT("Custom Save transfer")))
		{
			return false;
		}

		//Same as Copy and Cloud, a replaced primary starts a new Target history.
		if (!RemoveStaleBackup(TargetFile, TEXT("Custom Save transfer")))
		{
			return false;
		}

		UE_LOG(LogEasyMultiSave, Log, TEXT("Custom Save Object transferred: %s from Slot: %s to Slot: %s"),
			*CustomSaveName, *SourceSlot, *TargetSlot);
		return true;
	}

	UE_LOG(LogEasyMultiSave, Warning, TEXT("Failed to transfer Custom Save Object: %s"), *CustomSaveName);
	return false;
}

bool UEMSObjectBase::SaveAllCustomObjects()
{
	bool bAllSuccess = true;
	bool bAnyAttempted = false;

	for (auto& CustomObjectPair : CachedCustomSaves)
	{
		UEMSCustomSaveGame* SaveGame = CustomObjectPair.Value;
		if (!SaveGame)
		{
			continue;
		}

		//It can only auto-save objects in the current slot, but you can still manually save in other slots
		if (SaveGame->bUseSaveSlot)
		{
			//Skip slot-bound custom saves cached for a different slot, save those explicitly
			if (!EMS::StringEmpty(SaveGame->SlotName)
				&& !SaveGame->SlotName.Equals(GetCurrentSaveGameName(), ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		bAnyAttempted = true;
		bAllSuccess &= SaveCustom(SaveGame);
	}

	//Nothing to save is not a success, only true when all attempted saves succeeded
	return bAnyAttempted && bAllSuccess;
}

FCustomSaveInfo UEMSObjectBase::GetCustomSaveInfo(const UEMSCustomSaveGame* SaveGame) const 
{
	FCustomSaveInfo Info;  

	if (!IsValid(SaveGame))
	{
		Info.bValid = false; 
		return Info;
	}

	Info.bValid = true;
	Info.bUseSlot = SaveGame->bUseSaveSlot;
	Info.CustomSaveName = ValidateSaveName(SaveGame->SaveGameName);
	Info.SlotName = Info.bUseSlot ? ValidateSaveName(SaveGame->SlotName) : FString();
	Info.SaveFile = CustomSaveFile(Info.CustomSaveName, Info.SlotName);
	Info.CachedRefName = MakeCachedCustomSaveName(Info.CustomSaveName, Info.SlotName);

	return Info;
}

void UEMSObjectBase::DeleteCustomSave(UEMSCustomSaveGame* SaveGame)
{
	const FCustomSaveInfo Info = GetCustomSaveInfo(SaveGame);
	if (!Info.bValid)
	{
		return;
	}

	if (DeleteSaveFile(Info.SaveFile))
	{
		ClearCustomSaveByRef(Info.CachedRefName);

		if (Info.bUseSlot)
		{
			UE_LOG(LogEasyMultiSave, Log, TEXT("Custom Save Object deleted: %s from Slot: %s"), *Info.CustomSaveName, *Info.SlotName);
		}
		else
		{
			UE_LOG(LogEasyMultiSave, Log, TEXT("Custom Save Object deleted: %s"), *Info.CustomSaveName);
		}
	}
}

void UEMSObjectBase::ResetCustomSave(UEMSCustomSaveGame* SaveGame, const EResetCustomSaveType Type)
{
	const FCustomSaveInfo Info = GetCustomSaveInfo(SaveGame);
	if (!Info.bValid)
	{
		return;
	}

	/*
	The file identity is resolved by GetCustomSave, not by the class defaults.
	A reset must never redirect later saves to a different file, or to no file at all.
	*/
	auto ResetToDefaults = [SaveGame]()
	{
		const FString ResolvedName = SaveGame->SaveGameName;
		FSaveHelpers::ResetObjectToDefaults(SaveGame, false);
		SaveGame->SaveGameName = ResolvedName;
	};

	switch (Type)
	{
	case EResetCustomSaveType::Full:
		ClearCustomSaveByRef(Info.CachedRefName);
		ResetToDefaults();
		break;

	case EResetCustomSaveType::ClearMemory:
		ClearCustomSaveByRef(Info.CachedRefName);
		break;

	case EResetCustomSaveType::ResetDefault:
		ResetToDefaults();
		break;
	}

	UE_LOG(LogEasyMultiSave, Log, TEXT("Custom Save Object reset: %s with option %d"), *Info.CustomSaveName, uint8(Type));
}

void UEMSObjectBase::ClearCustomSaveByRef(const FName& CachedRefName)
{
	if (CachedRefName.IsNone())
	{
		return;
	}

	//Validated and used MakeCached...  already
	CachedCustomSaves.Remove(CachedRefName);
	UE_LOG(LogEasyMultiSave, Verbose, TEXT("Cached Custom Save removed: %s"), *CachedRefName.ToString());
}

void UEMSObjectBase::ClearCachedCustomSave(const FString& InSlotName, const FString& InFileName)
{
	if (EMS::StringEmpty(InFileName))
	{
		return;
	}

	//Used by Save Slot Copy. Must match the key GetCustomSave cached the object under.
	const FName CachedRefName = MakeCachedCustomSaveName(ValidateSaveName(InFileName), ValidateSaveName(InSlotName));
	ClearCustomSaveByRef(CachedRefName);
}

void UEMSObjectBase::ClearCachedCustomSaves()
{
	CachedCustomSaves.Empty();
}

void UEMSObjectBase::ClearCustomSavesDesktop(const FString& SaveGameName)
{
	//Only clears the cache
	TArray<FString> TempArray;
	ClearCustomSavesConsole(SaveGameName, false, TempArray);
}

void UEMSObjectBase::ClearCustomSavesConsole(const FString& SaveGameName, const bool bAddFiles, TArray<FString>& OutFiles)
{
	auto TempCustomSaves = CachedCustomSaves;

	for (auto It = TempCustomSaves.CreateIterator(); It; ++It)
	{
		const TObjectPtr<UEMSCustomSaveGame> CustomSaveGame = It->Value;
		if (!CustomSaveGame)
		{
			continue;
		}

		//Match slot directly. Slot1 and Slot10 must never be confused with each other
		if (!CustomSaveGame->SlotName.Equals(SaveGameName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		/*
		Cannot delete if the slot is not cached, non-cached custom saves must be removed manually on console.
		We cannot iterate the files on console!
		*/
		if (bAddFiles)
		{
			const FString CustomName = SaveGameName + EMS::UnderscoreFile + ValidateSaveName(CustomSaveGame->SaveGameName);
			OutFiles.Add(CustomName);
		}

		CachedCustomSaves.Remove(It->Key);
	}
}

bool UEMSObjectBase::DoesCustomSaveExist(const FString& InSlotName, const FString& InFileName) const
{
	if (IsFileNameReserved(InFileName, true))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Custom Save with reserved file name cannot exist: %s"), *InFileName);
		return false;
	}

	return DoesFileExist(CustomSaveFile(InFileName, InSlotName));
}

/**
Save Slots
**/

static FName MakeCachedSlotName(const FString& SlotName)
{
	return EMS::ToCanonicalFName(SlotName);
}

FString UEMSObjectBase::GetCurrentSaveGameName() const
{
	const FString SlotName = EMS::StringEmpty(CurrentSaveGameName)
		? ValidateSaveName(UEMSPluginSettings::Get()->DefaultSaveGameName)
		: CurrentSaveGameName;

	return EMS::StringEmpty(SlotName) || FSavePaths::IsSlotNameReserved(SlotName)
		? EMS::FallbackName
		: SlotName;
}

void UEMSObjectBase::SetCurrentSaveGameName(const FString& SaveGameName)
{
	const FString NewName = ValidateSaveName(SaveGameName);

	if (NewName.IsEmpty() || FSavePaths::IsSlotNameReserved(NewName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Invalid or reserved slot name rejected. Keeping: %s"), *GetCurrentSaveGameName());
		return;
	}

	if (!CurrentSaveGameName.Equals(NewName, ESearchCase::IgnoreCase))
	{
		if (IsAsyncTaskActive(false))
		{
			return;
		}

		/*
		Do NOT clear MultiSaveLevels here. Multi-Level session state
		(LevelArchiveList + MultiSaveLevels) is reset together manually via ClearMultiLevelSave
		So names stay in sync with the actual archive data.
		*/

		//When switching slots, we want to always load from disk
		ClearLoadFromMemory();

		CurrentSaveGameName = NewName;
		SaveConfig();

		UE_LOG(LogEasyMultiSave, Log, TEXT("New Current Save Game Slot is: %s"), *CurrentSaveGameName);
	}
}

TArray<FString> UEMSObjectBase::GetSortedSaveSlots() const
{
	TArray<FString> SaveGameNames;

	if (IsConsoleFileSystem())
	{
		//Files
		SaveGameNames = FSavePaths::GetConsoleSlotFiles(GetAllSaveGames());
	}
	else
	{
		//Folders
		IFileManager::Get().FindFiles(SaveGameNames, *FPaths::Combine(BaseSaveDir(), TEXT("*")), false, true);
	}

	//Return list sorted by time
	TArray<FSaveSlotInfo> SaveSlots = GetSlotInfos(SaveGameNames);
	return FSavePaths::GetSortedSaveSlots(SaveSlots);
}

TArray<FString> UEMSObjectBase::GetAllSaveGames() const
{
	TArray<FString> SaveGameNames;

	//Might not be available on all platforms
	ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
	if (SaveSystem)
	{
		SaveSystem->GetSaveGameNames(SaveGameNames, PlayerIndex);
	}

	return SaveGameNames;
}

TArray<FSaveSlotInfo> UEMSObjectBase::GetSlotInfos(const TArray<FString>& SaveGameNames) const
{
	//Fill with proper info
	TArray<FSaveSlotInfo> SaveSlots;
	for (const FString& SlotName : SaveGameNames)
	{
		FSaveSlotInfo SlotInfo;
		const FString SlotPath = SlotFilePath(SlotName);
		SlotInfo.TimeStamp = IFileManager::Get().GetTimeStamp(*SlotPath);
		SlotInfo.Name = SlotName;

		SaveSlots.Add(SlotInfo);
	}

	return SaveSlots;
}

UEMSInfoSaveGame* UEMSObjectBase::GetSlotInfoObject(const FString& SaveGameName)
{
	const FString ValidSaveName = EMS::StringEmpty(SaveGameName)
		? GetCurrentSaveGameName()
		: ValidateSaveName(SaveGameName);

	//Try to get a cached slot or create a new one
	UEMSInfoSaveGame* Slot = CachedSaveSlots.FindRef(MakeCachedSlotName(ValidSaveName));
	if (!Slot)
	{
		Slot = MakeSlotInfoObject(ValidSaveName);
		if (!Slot)
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Slot access failed: %s"), *SaveGameName);
			return nullptr;
		}

		CachedSaveSlots.Add(MakeCachedSlotName(ValidSaveName), Slot);
	}

	//Keep level list when switching slots
	if (IsStackBasedMultiLevelSave()
		&& ValidSaveName.Equals(GetCurrentSaveGameName(), ESearchCase::IgnoreCase))
	{
		Slot->KeepMultiLevelSave(MultiSaveLevels);
	}

	return Slot;
}

UEMSInfoSaveGame* UEMSObjectBase::MakeSlotInfoObject(const FString& SaveGameName)
{
	const FString SaveSlotFile = SlotInfoSaveFile(SaveGameName);

	//Workaround for when class cannot be loaded from DefaultEngine.ini in cooked builds
	FSoftClassPath SaveSlotClass = UEMSPluginSettings::Get()->SlotInfoSaveGameClass;
	if (!SaveSlotClass.IsValid())
	{
		SaveSlotClass = EMS::DefaultSlotClass;
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Slot Info Class could not be read from settings, using default."));
	}

	return CreateNewSaveObject<UEMSInfoSaveGame>(SaveSlotFile, SaveSlotClass);
}

void UEMSObjectBase::SaveSlotInfoObject(const FString& InSaveGameName, const bool bOverrideLevelData)
{
	if (InSaveGameName.IsEmpty())
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Trying to save slot with an empty name"));
		return;
	}

	const FString SaveGameName = ValidateSaveName(InSaveGameName);

	//Custom saves (bOverrideLevelData=false) can be prevented from redundant calls
	if (!bOverrideLevelData && !HasSlotSaveTimePassed(SaveGameName, EMS::ShortDelay))
	{
		return;
	}

	//VerifyOrCreateDirectory was removed, as it was a no-op. ISaveSystem->SaveGame handles this.

	//Retrieve or create save object
	UEMSInfoSaveGame* SaveGame = GetSlotInfoObject(SaveGameName);
	if (!SaveGame)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Invalid Save Object for: %s"), *SaveGameName);
		return;
	}

	//Update save info
	const FSaveSlotInfo PrevInfo = SaveGame->SlotInfo;
	SaveGame->SlotInfo.Name = SaveGameName;
	SaveGame->SlotInfo.TimeStamp = FDateTime::Now();

	//Custom Saves will update slot info (date) but never write Level Name or World data like Actors
	if (bOverrideLevelData)
	{
		const FName LevelName = GetSavedSlotLevelName();
		SaveGame->SlotInfo.Level = LevelName;

		//Maintain list of all saved level names
		if (IsStackBasedMultiLevelSave())
		{
			SaveGame->UpdateMultiLevelSave(LevelName, MultiSaveLevels);
		}

		TArray<FString> PlayerNames;
		FSaveHelpers::ExtractPlayerNames(GetWorld(), PlayerNames);
		SaveGame->SlotInfo.Players = PlayerNames;
	}

	//Save object
	if (SaveObject(*SlotInfoSaveFile(SaveGameName), SaveGame))
	{
		UE_LOG(LogEasyMultiSave, Log, TEXT("Slot Info saved: %s"), *SaveGameName);
		SaveGame->LastSaveAttemptTime = FPlatformTime::Seconds(); //On actual write
	}
	else
	{
		//Just keep previous one
		SaveGame->SlotInfo = PrevInfo; 
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Slot Info could not be saved: %s"), *SaveGameName);
	}
}


/*
Shared Target Slot mutation helpers.
A Target that received new data must show a fresh write, and its old backup must not outlive the replaced primary.
*/

bool UEMSObjectBase::StampSlotInfo(const FString& TargetSlot, const TCHAR* Operation)
{
	//The ordinary Slot Info save keeps Target metadata intact, forced past the short throttle to stamp the write.
	UEMSInfoSaveGame* TargetSlotInfo = GetSlotInfoObject(TargetSlot);
	if (!TargetSlotInfo)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("%s: Failed to load Target Slot Info: %s"), Operation, *TargetSlot);
		return false;
	}

	TargetSlotInfo->LastSaveAttemptTime = -EMS::ShortDelay;
	SaveSlotInfoObject(TargetSlot);

	//Only an actual write stamps a positive time
	if (TargetSlotInfo->LastSaveAttemptTime < 0.0)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("%s: Failed to update Target Slot Info: %s"), Operation, *TargetSlot);
		return false;
	}

	return true;
}

bool UEMSObjectBase::RemoveStaleBackup(const FString& TargetPath, const TCHAR* Operation) const
{
	const FFileContext BackupContext(PlayerIndex, FSavePaths::GetBackupSavePath(TargetPath));
	if (!FSaveFileIntegrity::DoesSaveExist(BackupContext))
	{
		return true;
	}

	if (FSaveFileIntegrity::DeleteSaveFile(BackupContext))
	{
		return true;
	}

	UE_LOG(LogEasyMultiSave, Error, TEXT("%s: Failed to remove stale Target backup: %s"), Operation, *BackupContext.Path);
	return false;
}

bool UEMSObjectBase::WasCurrentLevelSaved() 
{
	//See if this level was saved once(used to prevent the wait node from waiting too long)
	const FString SaveGameName = GetCurrentSaveGameName();
	if (const UEMSInfoSaveGame* SaveGame = GetSlotInfoObject(SaveGameName))
	{
		const FName LevelName = GetSavedSlotLevelName();
		return SaveGame->HasLevel(LevelName);
	}

	return false;
}

void UEMSObjectBase::ClearMultiSaveLevels()
{
	const FString SaveGameName = GetCurrentSaveGameName();
	UEMSInfoSaveGame* SaveGame = GetSlotInfoObject(SaveGameName);
	if (SaveGame)
	{
		MultiSaveLevels.Empty();
		SaveGame->SlotInfo.Levels.Empty();
	}
}

bool UEMSObjectBase::HasSlotSaveTimePassed(const FString& SaveGameName, const double InTime) const
{
	if (EMS::StringEmpty(SaveGameName))
	{
		return true;
	}

	const FName ValidCacheName = MakeCachedSlotName(ValidateSaveName(SaveGameName));
	if (const UEMSInfoSaveGame* SaveGame = CachedSaveSlots.FindRef(ValidCacheName))
	{
		return (FPlatformTime::Seconds() - SaveGame->LastSaveAttemptTime) >= InTime;
	}

	return true;
}

bool UEMSObjectBase::DoesSaveSlotExist(const FString& SaveGameName) const
{
	return DoesFileExist(SlotInfoSaveFile(SaveGameName));
}

bool UEMSObjectBase::DoesFullSaveGameExist(const FString& SaveGameName) const
{
	const bool bHasSlotFile = DoesFileExist(SlotInfoSaveFile(SaveGameName));
	const bool bHasPlayerFile = DoesFileExist(PlayerSaveFile(SaveGameName));
	const bool bHasLevelFile = DoesFileExist(ActorSaveFile(SaveGameName));

	return bHasSlotFile && bHasPlayerFile && bHasLevelFile;
}

void UEMSObjectBase::ClearCachedSaveSlot(const FString& SlotName)
{
	if (EMS::StringEmpty(SlotName))
	{
		return;
	}

	//Must match the canonical key used by GetSlotInfoObject.
	CachedSaveSlots.Remove(MakeCachedSlotName(ValidateSaveName(SlotName)));
}

void UEMSObjectBase::DeleteAllSaveDataForSlot(const FString& InSaveGameName)
{
	if (EMS::StringEmpty(InSaveGameName))
	{
		return;
	}

	//A running task may be writing these files right now
	if (IsAsyncTaskActive())
	{
		return;
	}

	/*
	Multi-Level save is explicitly not cleared when level slot data is removed
	If you switch slots, then delete one etc. you dont want all level data to be gone
	*/

	const FString SaveGameName = ValidateSaveName(InSaveGameName);

	//Never let a reserved name resolve onto the User folder or a backup path and delete it
	if (FSavePaths::IsSlotNameReserved(SaveGameName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Reserved Slot name rejected for deletion: %s"), *SaveGameName);
		return;
	}

	//Console uses files and not folders
	if (IsConsoleFileSystem())
	{
		//Hardcoded default files, since we cannot iterate through them
		TArray<FString> AllFiles = FSavePaths::GetDefaultSaveFiles(SaveGameName);
		bool bSuccess = false;

		//Parse the custom save objects and clear their cache
		ClearCustomSavesConsole(SaveGameName, true, AllFiles);

		//Use native delete 
		for (const FString& FileName : AllFiles)
		{
			bSuccess |= DeleteSaveFile(FileName);
		}

		if (bSuccess)
		{
			UE_LOG(LogEasyMultiSave, Log, TEXT("Save Game Data removed for: %s"), *SaveGameName);
		}
	}
	//Folders
	else
	{
		FSavePaths::DeleteSaveDirectory(BaseSaveDir(), SaveGameName, "Save Slot");

		//Delete the cached custom save objects
		ClearCustomSavesDesktop(SaveGameName);
	}

	//Keep in sync with disk, or a later save re-writes the deleted data
	CachedSaveSlots.Remove(MakeCachedSlotName(SaveGameName));
	if (SaveGameName.Equals(GetCurrentSaveGameName(), ESearchCase::IgnoreCase))
	{
		ClearLoadFromMemory();
	}
}

void UEMSObjectBase::DeleteDataFromSlot(const FString& InSaveGameName, const EDeleteSlotType InData)
{
	if (InData == EDeleteSlotType::Full)
	{
		//Handled in Blueprint lib with DeleteAllSaveDataForSlot
		return;
	}

	if (EMS::StringEmpty(InSaveGameName))
	{
		return;
	}

	//A running task may be writing these files right now
	if (IsAsyncTaskActive())
	{
		return;
	}

	const FString ValidName = ValidateSaveName(InSaveGameName);
	if (FSavePaths::IsSlotNameReserved(ValidName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Reserved Slot name rejected for deletion: %s"), *ValidName);
		return;
	}

	const bool bIsPlayer = InData == EDeleteSlotType::Player;
	const FString SlotFile = bIsPlayer ? PlayerSaveFile(ValidName) : ActorSaveFile(ValidName);
	const bool bSuccess = DeleteSaveFile(SlotFile);

	if (bSuccess)
	{
		//Keep in sync with disk, or a later save re-writes the deleted data
		CachedSaveSlots.Remove(MakeCachedSlotName(ValidName));
		if (ValidName.Equals(GetCurrentSaveGameName(), ESearchCase::IgnoreCase))
		{
			ClearLoadFromMemory();
		}

		const FString DataType = bIsPlayer ? TEXT("Player") : TEXT("Level");
		UE_LOG(LogEasyMultiSave, Log, TEXT("%s Data removed for: %s"), *DataType, *ValidName);
	}
}

void UEMSObjectBase::ClearCachedSlots()
{
	CachedSaveSlots.Empty();
}

/**
Save Users
**/

FString UEMSObjectBase::GetCurrentSaveUserName() const
{
	return CurrentSaveUserName;
}

bool UEMSObjectBase::HasSaveUserName() const
{
	return !GetCurrentSaveUserName().IsEmpty();
}

void UEMSObjectBase::SetCurrentSaveUserName(const FString& UserName)
{
	const bool bIsConsole = IsConsoleFileSystem();
	const bool bRequestedEmpty = UserName.IsEmpty();

	//Both console and empty input result in clearing
	if (bIsConsole || bRequestedEmpty)
	{
		if (!CurrentSaveUserName.IsEmpty())
		{
			if (IsAsyncTaskActive(false))
			{
				return;
			}

			ClearUserData();
			CurrentSaveUserName.Reset();
			SaveConfig();
		}

		UE_LOG(LogEasyMultiSave, Log, TEXT("Save User Name Cleared"));

		if (bIsConsole)
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Save Users are not supported when using the console file system."));
		}

		return;
	}

	const FString NewName = ValidateSaveName(UserName);
	if (!CurrentSaveUserName.Equals(NewName, ESearchCase::IgnoreCase))
	{
		if (IsAsyncTaskActive(false))
		{
			return;
		}

		ClearUserData();
		CurrentSaveUserName = NewName;
		SaveConfig();

		UE_LOG(LogEasyMultiSave, Log, TEXT("New Current Save User Name is: %s"), *NewName);
	}
}

void UEMSObjectBase::DeleteAllSaveDataForUser(const FString& UserName)
{
	if (EMS::StringEmpty(UserName))
	{
		return;
	}

	if (IsAsyncTaskActive(false))
	{
		return;
	}

	FSavePaths::DeleteSaveDirectory(SaveUserDir(), ValidateSaveName(UserName), "Save User");

	//We only clear all to be super-safe here not just for the current user
	ClearUserData();
}

TArray<FString> UEMSObjectBase::GetAllSaveUsers() const
{
	TArray<FString> SaveUserNames;
	IFileManager::Get().FindFiles(SaveUserNames, *FPaths::Combine(SaveUserDir(), TEXT("*")), false, true);
	return SaveUserNames;
}

void UEMSObjectBase::ClearUserData()
{
	ClearCachedSlots();
	ClearCachedCustomSaves();
	ClearLoadFromMemory();
}

/**
Save Archive Functions
**/

bool UEMSObjectBase::SaveBinaryArchive(const FString& FullSavePath, FBufferArchive& BinaryData) const
{

#if EMS_PLATFORM_DESKTOP
	//Ensure the file is not read-only on desktop platforms
	FSavePaths::CheckForReadOnly(FullSavePath);
#endif

	//Handle automatic backup before overwriting existing data
	if (UEMSPluginSettings::Get()->bAutoBackup)
	{
		const FFileContext Context = FFileContext(PlayerIndex, FullSavePath);
		FSaveFileIntegrity::PerformAutoBackup(Context);
	}

	return SaveBinaryData(FullSavePath, MoveTemp(BinaryData));
}

bool UEMSObjectBase::SaveBinaryData(const FString& FullSavePath, TArray<uint8> InData) const
{
	//Save data now will always compress regardless of the file system.

	//Compression Logic
	TArray<uint8> DataToSave;
	FArchiveSaveCompressedProxy Compressor(DataToSave, NAME_Oodle);

	if (Compressor.GetError() || Compressor.IsCriticalError())
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Compression failed for: %s"), *FullSavePath);
		return false;
	}

	Compressor << InData;
	Compressor.Flush();

	if (!Compressor.Close() || Compressor.IsCriticalError())
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Compression failed during serialization for: %s"), *FullSavePath);
		return false;
	}

	//Actual File IO
	const bool bSaveSuccess = CheckAndSaveFile(*FullSavePath, DataToSave);
	if (!bSaveSuccess)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Failed to save file: %s"), *FullSavePath);
	}

	return bSaveSuccess;
}

/**
Load Archive Functions
**/

bool UEMSObjectBase::ParseArchiveData(const FLoadArchiveContext& Context, const TArray<uint8>& InData)
{
	FMemoryReader MemoryReader(InData, true);

	//Now apply version to package initially
	ApplyLastFileVersion(MemoryReader, true);

	const FString FullSavePath = Context.SavePath; 

	//Check for internal archive errors
	if (MemoryReader.IsError() || MemoryReader.IsCriticalError())
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Unknown archive error. The Data is valid, but something went wrong! File: %s"), *FullSavePath);
		return false;
	}

	bool bWasLoaded = false;

	//Directly load Object
	if (Context.LoadType == EDataLoadType::DATA_Object)
	{
		bWasLoaded = LoadObject(MemoryReader, Context.GetSingleObject());
	}
	//Process Collection
	else if (Context.LoadType == EDataLoadType::DATA_Collection)
	{
		bWasLoaded = ProcessObjectCollection(MemoryReader, Context.GetObjects());
	}
	//Complex Actor archive unpacking for Level and Player
	else
	{
		bWasLoaded = UnpackActorArchive(MemoryReader, Context.LoadType);
	}

	if (!bWasLoaded)
	{
		//This only means level does not match etc. By this time, the files are valid
		UE_LOG(LogEasyMultiSave, Log,
			TEXT("Data is valid, but was not parsed for logical reasons. Usually a different Level without (Persistent or Full) Multi-Level Saving. File: %s"),
			*FullSavePath);
	}

	return bWasLoaded;
}

bool UEMSObjectBase::LoadBinaryArchive(const FLoadArchiveContext& Context)
{
	const FString& FullSavePath = Context.SavePath; //Use Reference
	if (FullSavePath.IsEmpty())
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Load Binary Archive has empty path!"));
		return false;
	}

	//Early out, as missing files are ignored and won't be restored from backup
	if (!DoesFileExist(*FullSavePath))
	{
		return false;
	}

	TArray<uint8> BinaryData;
	EFileValidity Result;
	bool bIsRestoredFromBackup = false;

	//Attempt Primary Load
	if (!LoadBinaryData(FullSavePath, BinaryData, Result))
	{
		//Stop here if the file is technically incompatible
		if (Result == EFileValidity::FILE_INCOMPATIBLE)
		{
			return false;
		}

		const FString BackupPath = FSavePaths::GetBackupSavePath(FullSavePath);
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Primary file corrupt, trying backup: %s"),*BackupPath);

		//Attempt Backup
		if (!LoadBinaryData(BackupPath, BinaryData, Result))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Load failed: No valid data in Primary or Backup for %s"), *FullSavePath);
			return false;
		}

		bIsRestoredFromBackup = true;
	}

	//Parse Data
	const bool bParsed = ParseArchiveData(Context, BinaryData);
	if (bParsed && bIsRestoredFromBackup)
	{
		UE_LOG(LogEasyMultiSave, Log, TEXT("Successfully restored data from Backup for: %s"), *FullSavePath);
	}

	return bParsed;
}

bool UEMSObjectBase::LoadBinaryData(const FString& FullSavePath, TArray<uint8>& OutBinaryData, EFileValidity& OutResult) const
{
	//Initial File Load and Integrity Check
	TArray<uint8> RawFileData;
	FSaveHeader FileHeader;
	OutResult = CheckAndLoadFile(FullSavePath, RawFileData, FileHeader);

	if (OutResult != EFileValidity::FILE_VALID)
	{
		return false;
	}

	/*
	Newer headers have a compression flag for consoles(so old uncompressed data is supported)
	Desktop files have always been compressed in every version. 
	In the new code, it will compress all files on save, regardless of the system.
	OODLE is platform agnostic and due to file limitations on console it makes sense. 
	*/
	const bool bIsFileDecompressionAllowed = FileHeader.IsDataCompressed() || !IsConsoleFileSystem();

	//Decompression logic
	if (bIsFileDecompressionAllowed)
	{
		FArchiveLoadCompressedProxy Decompressor(RawFileData, NAME_Oodle);

		if (Decompressor.GetError() || Decompressor.IsCriticalError())
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Decompression failed for: %s"), *FullSavePath);
			return false;
		}

		TArray<uint8> DecompressedBuffer;
		Decompressor << DecompressedBuffer;

		if (!Decompressor.Close() || Decompressor.IsCriticalError())
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Decompression failed during serialization for: %s"), *FullSavePath);
			return false;
		}

		//Transfer data without copying the entire array
		OutBinaryData = MoveTemp(DecompressedBuffer);
	}
	else
	{
		//Direct move on console legacy files, when no compression is applied
		OutBinaryData = MoveTemp(RawFileData);
	}

	return !OutBinaryData.IsEmpty();
}

/**
File I/O functions 
Integrated with Versioning
**/

EFileValidity UEMSObjectBase::CheckAndLoadFile(const FString& FullSavePath, TArray<uint8>& OutData, FSaveHeader& OutHeader) const
{
	const FFileContext Context = FFileContext(PlayerIndex, FullSavePath);
	const EFileValidity File = FSaveFileIntegrity::CheckAndLoadFile(Context, OutData, OutHeader);
	const bool bIsValid = File == EFileValidity::FILE_VALID;

	if (!bIsValid)
	{
		return File;
	}

	//Early check for incompatible multi level modes, which can cause a crash. New files only.
	const bool bMultiLevelCompat = FSaveFileIntegrity::HasMultiLevelCompatibility(Context, OutHeader);
	if (!bMultiLevelCompat)
	{
		return EFileValidity::FILE_INCOMPATIBLE;
	}

	/*
	Init last read version. We keep this global because we assume that all files are from the same version anyway.
	But if not, it is always correctly scoped to "the file currently being parsed"
	Each read-header-then-deserialize cycle fully completes before the next.
	Reading Custom/Collection concurrent to Multi-Thread load could in theory overwrite it.
	But as stated above, files are mostly from the same version.
	*/
	OutHeader.InitVersionInfo(LastFileVersion, LastGameVersion);

	return File;
}

bool UEMSObjectBase::CheckAndSaveFile(const FString& FullSavePath, const TArray<uint8>& InData) const
{
	const FFileContext Context = FFileContext(PlayerIndex, FullSavePath, LastFileVersion.GetLegacyVer());
	return FSaveFileIntegrity::CheckAndSaveFile(Context, InData);
}

bool UEMSObjectBase::DeleteSaveFile(const FString& FullSavePath) const
{
	const FFileContext Context = FFileContext(PlayerIndex, FullSavePath);
	return FSaveFileIntegrity::DeleteSaveFile(Context);
}

 bool UEMSObjectBase::DoesFileExist(const FString& FullSavePath) const
{
	const FFileContext Context = FFileContext(PlayerIndex, FullSavePath);
	return FSaveFileIntegrity::DoesSaveExist(Context);
}

/**
Versioning Functions
**/

EIntegrityCheckResult UEMSObjectBase::CheckSaveGameIntegrity(const FString& FullSavePath) const
{
	const FFileContext Context = FFileContext(PlayerIndex, FullSavePath);
	return FSaveFileIntegrity::CheckSaveGameIntegrity(Context);
}

void UEMSObjectBase::ApplyLastFileVersion(FMemoryReader& MemoryReader, const bool bHandleLegacy) const
{
	//Check for legacy versions. Must be checked here as those may be in bulk data block(now center)
	if (bHandleLegacy)
	{
		FSaveVersion::HandleLegacyFileVersion(MemoryReader, LastFileVersion);
	}

	FSaveVersion::ApplyFileVersionInfo(LastFileVersion, MemoryReader);
}

void UEMSObjectBase::GetGameVersion(int32& SaveFileVersion, int32& SettingsVersion) const
{
	SettingsVersion = FSaveVersion::GetSettingsGameVersionAsInt();
	SaveFileVersion = LastGameVersion.GetGameVal();
}

/**
Save and Load Complete Objects(Slot, Custom Save)
**/

bool UEMSObjectBase::SaveObject(const FString& FullSavePath, UObject* SaveGameObject) const
{
	if (!SaveGameObject)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("SaveObject: Invalid SaveGameObject."));
		return false;
	}

	//Serialize with version into Data
	TArray<uint8> Data;
	SerializeToBinary(SaveGameObject, Data, FSerializeMeta::CompleteObject());

	//Now wrap the filled Data with buffer archive
	FBufferArchive Archive;
	Archive << Data;

	//Check and save
	if (!FSaveHelpers::HasSaveArchiveError(Archive, ESaveErrorType::ER_Object))
	{
		return SaveBinaryArchive(*FullSavePath, Archive);
	}

	return false;
}

bool UEMSObjectBase::LoadObject(FMemoryReader& FromBinary, UObject* SaveGameObject) const
{
	if (!SaveGameObject)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Load Object: Invalid object"));
		return false;
	}

	//Unwrap from buffer archive
	FBufferArchive Archive;
	FromBinary << Archive;

	if (FromBinary.IsError() || FromBinary.IsCriticalError())
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Load Object: Archive error"));
		return false;
	}

	//Deserialize
	SerializeFromBinary(SaveGameObject, Archive, FSerializeMeta::CompleteObject());

	return true;
}

/**
Base Game-Object Serialize Functions
**/

void UEMSObjectBase::SerializeToBinary(const UObject* Object, TArray<uint8>& OutData, const FSerializeMeta& Meta) const
{
	FMemoryWriter MemoryWriter(OutData, true);
	UObject* ObjectRef = const_cast<UObject*>(Object);
	SerializeObjectWithVersion(ObjectRef, MemoryWriter, Meta.Type);

	if (UEMSPluginSettings::Get()->bDebugDataSize)
	{
		FSaveLogDebug::DebugLogDataSize(Object, OutData);
	}

	if (!Meta.bSkipDebug)
	{
		WriteSaveReport(FSaveReportContext::ForObject(Object, Meta.Type, false));
	}
}

void UEMSObjectBase::SerializeFromBinary(UObject* Object, const TArray<uint8>& InData, const FSerializeMeta& Meta) const
{
	FMemoryReader MemoryReader(InData, true);
	DeserializeObjectWithVersion(Object, MemoryReader, Meta.Type);

	if (!Meta.bSkipDebug)
	{
		WriteSaveReport(FSaveReportContext::ForObject(Object, Meta.Type, true));
	}
}

/**
Core Serialize Helpers
**/
 
void UEMSObjectBase::SerializeObjectWithVersion(UObject* Object, FMemoryWriter& Writer, const ESaveObjectType Type)
{
	uint32 ObjectTag = EMS::OBJECT_INST_TAG;
	uint32 ObjectVer = EMS::OBJECT_INST_VERSION_LATEST;
	FPackageFileVersion ObjectFileVersion = GPackageFileUEVersion;

	Writer << ObjectTag;
	Writer << ObjectVer;
	Writer << ObjectFileVersion;

	const FCoreArchiveContext Context = FCoreArchiveContext(Type, ObjectVer, Object);
	FCoreSaveArchive Ar(Writer, Context);
	Object->Serialize(Ar);
}

void UEMSObjectBase::DeserializeObjectWithVersion(UObject* Object, FMemoryReader& Reader, const ESaveObjectType Type) const
{
	//Generic engine and file version
	ApplyLastFileVersion(Reader);

	uint32 ObjectTag = 0;
	uint32 ObjectVer = 0;

	Reader << ObjectTag;

	//Newer data with version header
	if (ObjectTag == EMS::OBJECT_INST_TAG)
	{
		Reader << ObjectVer;

		//From here on, we add the actual version
		if (ObjectVer >= EMS::OBJECT_INST_VERSION_2)
		{
			FPackageFileVersion ObjectFileVersion;
			Reader << ObjectFileVersion;
			Reader.SetUEVer(ObjectFileVersion);
		}
	}
	//Legacy data without header
	else
	{
		Reader.Seek(0);

		//This is only valid for files from 177 where Complete Objects dont have the data version yet
		if (Type != ESaveObjectType::CompleteObject)
		{
			Reader.SetUEVer(LastFileVersion.GetLegacyVer()); 
		}
	}

	const FCoreArchiveContext Context = FCoreArchiveContext(Type, ObjectVer, Object);
	FCoreSaveArchive Ar(Reader, Context);
	Object->Serialize(Ar);
}

/**
Advanced Save Report
**/

#if EMS_BUILD_DEVELOPER
void UEMSObjectBase::WriteSaveReport(const FSaveReportContext& Context) const
{
	if (FSettingHelpers::IsSaveReportEnabled())
	{
		//Create the context
		FSaveReportContext DebugContext = Context;

		DebugContext.SlotName = GetCurrentSaveGameName();
		DebugContext.UserName = GetCurrentSaveUserName();

		const bool bLoad = Context.bIsLoading;
		DebugContext.PluginVersion =
			bLoad ? LastGameVersion.GetPluginVer() : FSaveVersion::GetStaticPluginVersion();
		DebugContext.GameVersion =
			bLoad ? LastGameVersion.GetGameVer() : FSaveVersion::GetSettingsGameVersion();

		//In case we are using Multi-Thread saving/loading we need to pass it to the GameThread
		if (!IsInGameThread())
		{
			AsyncTask(ENamedThreads::GameThread, [DebugContext]()
			{
				if (IsValid(DebugContext.TargetObject))
				{
					FSaveReport::WriteSaveReport(DebugContext);
				}
			});
		}
		else
		{
			FSaveReport::WriteSaveReport(DebugContext);
		}
	}
}

void UEMSObjectBase::CommitSaveReportOnShutdown() const
{
	//Write debug info and evict cache
	FSaveReport::CommitSaveReport();
	FSaveReport::ClearReportCache();
}

void UEMSObjectBase::ClearReportCache(const bool bFullClear) const
{
	FSaveReport::ClearReportCache(bFullClear);
}
#endif

/**
File System
**/

FString UEMSObjectBase::ValidateSaveName(const FString& SaveGameName) const
{
	return FSavePaths::ValidateSaveName(SaveGameName);
}

FString UEMSObjectBase::SaveUserDir() const
{
	return FPaths::ProjectSavedDir() + EMS::SaveGamesFolder + EMS::UserDir + EMS::Slash;
}

FString UEMSObjectBase::ValidatedUserName() const
{
	return ValidateSaveName(GetCurrentSaveUserName());
}

FString UEMSObjectBase::UserSubDir() const
{
	//Takes into account the already defined path from ISaveGame
	return EMS::UserDir + EMS::Slash + ValidatedUserName() + EMS::Slash;
}

FString UEMSObjectBase::BaseSaveDir() const
{
	if (HasSaveUserName())
	{
		return SaveUserDir() + ValidatedUserName() + EMS::Slash;
	}

	return FPaths::ProjectSavedDir() + EMS::SaveGamesFolder;
}

FString UEMSObjectBase::BaseSavePath(const FString& FullSavePath) const
{
	/*
	FullSavePath already will have user included. Like ActorSaveFile().
	So we don't use BaseSaveDir here
	*/
	return FSavePaths::GetBaseSavePath(FullSavePath);
}

FString UEMSObjectBase::GetThumbnailFormat() const
{
	return FSavePaths::GetThumbnailFormat();
}

FString UEMSObjectBase::GetFolderOrFile() const
{
	//Console uses file names and not folders, "/" will automatically create a new folder.
	const bool bFile = IsConsoleFileSystem();
	const FString FolderOrFile = bFile ? EMS::UnderscoreFile : EMS::Slash;

	return FolderOrFile;
}

FString UEMSObjectBase::FullSaveDir(const FString& DataType, FString SaveGameName) const
{
	if (EMS::StringEmpty(SaveGameName))
	{
		SaveGameName = GetCurrentSaveGameName();
	}

	//Single canonical point
	SaveGameName = ValidateSaveName(SaveGameName);

	//sav is added by ISaveInterface
	const FString FullName = SaveGameName + GetFolderOrFile() + DataType;

	if (HasSaveUserName())
	{
		return UserSubDir() + FullName;
	}

	return FullName;
}

FString UEMSObjectBase::CustomSaveFile(const FString& InCustomSaveName, const FString& SlotName) const
{
	//Becomes the DataType in FullSaveDir, which does not validate it on its own
	FString CustomSaveName = ValidateSaveName(InCustomSaveName);

	//A custom file must never resolve onto a core slot file (Player/Level/Slot/Thumb)
	if (IsFileNameReserved(CustomSaveName))
	{
		CustomSaveName = EMS::FallbackName + EMS::UnderscoreFile + CustomSaveName;
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Custom Save File with reserved name was renamed to: %s"), *CustomSaveName);
	}

	//Bound to a save slot, use default dir.
	if (!EMS::StringEmpty(SlotName))
	{
		return FullSaveDir(CustomSaveName, SlotName);
	}

	//Not bound to slot, so we just save in the base folder. with user if desired.
	if (HasSaveUserName())
	{
		return UserSubDir() + CustomSaveName;
	}

	return CustomSaveName;
}

FString UEMSObjectBase::SlotInfoSaveFile(const FString& SaveGameName) const
{
	return FullSaveDir(EMS::SlotSuffix, SaveGameName);
}

FString UEMSObjectBase::ActorSaveFile(const FString& SaveGameName) const
{
	return FullSaveDir(EMS::ActorSuffix, SaveGameName);
}

FString UEMSObjectBase::PlayerSaveFile(const FString& SaveGameName) const
{
	return FullSaveDir(EMS::PlayerSuffix, SaveGameName);
}

FString UEMSObjectBase::ThumbnailSaveFile(const FString& SaveGameName) const
{
	return FullSaveDir(EMS::ThumbSuffix, SaveGameName);
}

FString UEMSObjectBase::SlotFilePath(const FString& SaveGameName) const
{
	//SlotFilePath only used for sorting.
	return BaseSavePath(SlotInfoSaveFile(SaveGameName));
}

FName UEMSObjectBase::GetSavedSlotLevelName() const
{
	//Return the name without redirectors. Used for slot to always save the current level
	const FString WorldLevel = FLevelHelpers::GetWorldLevelName(GetWorld(), false);
	return FName(WorldLevel);
}

bool UEMSObjectBase::IsFileNameReserved(const FString& SaveGameName, const bool bValidate) const
{
	if (bValidate)
	{
		return FSavePaths::IsFileNameReserved(ValidateSaveName(SaveGameName));
	}

	return FSavePaths::IsFileNameReserved(SaveGameName);
}

/**
Thumbnail Saving
Export from a 2d scene capture render target source.
No versioning etc. as this would be overkill.
**/

UTexture2D* UEMSObjectBase::ImportSaveThumbnail(const FString& SaveGameName) const
{
	if (EMS::StringEmpty(SaveGameName))
	{
		return nullptr;
	}

	TArray<uint8> Buffer;
	const FString SaveThumbnailName = ThumbnailSaveFile(SaveGameName);

	//Use raw load
	const FFileContext Context = FFileContext(PlayerIndex, SaveThumbnailName);
	const bool bLoadThumb = FSaveFileIntegrity::LoadFromFile(Context, Buffer) == EFileValidity::FILE_VALID;

	if (bLoadThumb)
	{
		UTexture2D* NewTexture = FImageUtils::ImportBufferAsTexture2D(Buffer);
		if (!NewTexture)
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Import Save Thumbnail: Error creating texture for %s"), *SaveThumbnailName);
		}	

		return NewTexture;
	}

	//Try Legacy(saved as image) Thumbnail afterwards
	if (!IsConsoleFileSystem())
	{
		const FString ThumbnailPath = BaseSaveDir() + ValidateSaveName(SaveGameName) + GetFolderOrFile();
		const FString LegacyBaseName = ThumbnailPath + EMS::ThumbSuffix;

		//Legacy thumbnails were written with image file ext whose format may differ current ThumbnailFormat setting
		const bool bPng = GetThumbnailFormat() == EMS::ImgFormatPNG;
		const FString CurrentExt = TEXT(".") + (bPng ? EMS::ImgFormatPNG : EMS::ImgFormatJPG);
		const FString AlternateExt = TEXT(".") + (bPng ? EMS::ImgFormatJPG : EMS::ImgFormatPNG);

		for (const FString& Ext : { CurrentExt, AlternateExt })
		{
			const FString LegacyThumbnailName = LegacyBaseName + Ext;
			if (FPaths::FileExists(LegacyThumbnailName))
			{
				return FImageUtils::ImportFileAsTexture2D(LegacyThumbnailName);
			}
		}
	}

	UE_LOG(LogEasyMultiSave, Warning, TEXT("Import Save Thumbnail: Failed to load data for %s"), *SaveThumbnailName);

	return nullptr;
}

void UEMSObjectBase::ExportSaveThumbnail(UTextureRenderTarget2D* TextureRenderTarget, const FString& SaveGameName) const
{
	if (!TextureRenderTarget || EMS::StringEmpty(SaveGameName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Export Save Thumbnail: Null RT or Empty Name."));
		return;
	}

	const FString ValidSaveGameName = ValidateSaveName(SaveGameName);
	if (FSavePaths::IsSlotNameReserved(ValidSaveGameName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Export Save Thumbnail: Reserved Slot name rejected: %s"), *ValidSaveGameName);
		return;
	}

	//Extract image data on the game thread NOW.
	FImage ExportImage;
	if (!FImageUtils::GetRenderTargetImage(TextureRenderTarget, ExportImage))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Export Save Thumbnail: Failed to extract Image data."));
		return;
	}

	const FString SaveThumbnailPath = ThumbnailSaveFile(ValidSaveGameName);

	//Check if multi-threaded saving is enabled
	if (FSettingHelpers::IsMultiThreadSaving())
	{
		TWeakObjectPtr<const UEMSObjectBase> WeakThis(this);

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakThis, SaveThumbnailPath, Image = MoveTemp(ExportImage)]() mutable
		{
			if (const UEMSObjectBase* StrongThis = WeakThis.Get())
			{
				StrongThis->ExecuteSaveThumbnail(SaveThumbnailPath, Image);
			}
			else
			{
				UE_LOG(LogEasyMultiSave, Verbose, TEXT("Export Save Thumbnail: Object destroyed before async save completed."));
			}
		});
	}
	else
	{
		ExecuteSaveThumbnail(SaveThumbnailPath, ExportImage);
	}
}

void UEMSObjectBase::ExecuteSaveThumbnail(const FString& ThumbnailPath, FImage& ImageToCompress) const
{
	FBufferArchive Buffer;
	if (FSaveThumbnails::CompressImage(ImageToCompress, Buffer))
	{
		const FFileContext Context = FFileContext(PlayerIndex, ThumbnailPath);
		if (!FSaveFileIntegrity::SaveThumbnailToFile(Context, Buffer))
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Export Save Thumbnail: Save failed."));
		}
	}
}
