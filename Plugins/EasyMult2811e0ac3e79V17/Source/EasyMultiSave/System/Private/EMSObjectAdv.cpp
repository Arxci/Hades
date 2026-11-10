//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSObjectAdv.h"
#include "EMSCustomSaveGame.h"
#include "EMSDebug.h"
#include "EMSPaths.h"
#include "EMSVersion.h"
#include "EMSObject.h"
#include "EMSInfoSaveGame.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BufferArchive.h"

/**
Object Collection
This is kept separate from Actor Saving.
It will also provide the base for future Unreal features like SceneGraph.
**/

bool UEMSObjectAdv::SaveObjectCollection(const TArray<FRawObjectSaveData>& Objects, bool bUseSlot, FString FileName)
{
	if (Objects.IsEmpty())
	{
		return false;
	}

	const FString FullPath = GetObjectCollectionPath(bUseSlot, FileName);

	//Add valid objects
	TMap<FString, TArray<uint8>> Serialized;
	for (const FRawObjectSaveData& Entry : Objects)
	{
		if (!Entry.IsValidData())
		{
			continue;
		}

		TArray<uint8> Data;
		FStructHelpers::SerializeStruct(Entry.Object);
		SerializeToBinary(Entry.Object, Data, FSerializeMeta::Collection());

		//Save Report for collection
		WriteSaveReport(FSaveReportContext::ForCollection(Entry.Object, FileName, bUseSlot, false));

		if (!Data.IsEmpty())
		{
			Serialized.Add(Entry.Id, MoveTemp(Data));
		}
	}

	if (Serialized.IsEmpty())
	{
		return false;
	}

	FBufferArchive Archive;
	Archive << Serialized;

	//Check for error and proceed
	bool bSuccess = false;
	if (!FSaveHelpers::HasSaveArchiveError(Archive, ESaveErrorType::ER_Collection))
	{
		bSuccess = SaveBinaryArchive(*FullPath, Archive);
	}

	if (bSuccess)
	{
		//Update slot from main, since data in it was modified
		if (bUseSlot)
		{
			if (UEMSObject* EMSMain = UEMSObject::Get(this))
			{
				EMSMain->SaveSlotInfoObject(EMSMain->GetCurrentSaveGameName());
			}
		}

		const FString SlotMessage = bUseSlot ? TEXT("to Slot") : TEXT("");
		UE_LOG(LogEasyMultiSave, Log, TEXT("Object Collection saved %s"), *SlotMessage);
	}

	return bSuccess;
}

bool UEMSObjectAdv::LoadObjectCollection(const TArray<FRawObjectSaveData>& Objects, bool bUseSlot, FString FileName)
{
	const FString FullPath = GetObjectCollectionPath(bUseSlot, FileName);

	FSaveObjects AllObjects;
	AllObjects.AddFromRaw(Objects);

	//Load from top level Binary archive as usual
	const FLoadArchiveContext Context = FLoadArchiveContext(FullPath, EDataLoadType::DATA_Collection, AllObjects);
	const bool bLoadBinary = LoadBinaryArchive(Context);

	if (bLoadBinary)
	{
		const FString SlotMessage = bUseSlot ? TEXT("from Slot") : TEXT("");
		UE_LOG(LogEasyMultiSave, Log, TEXT("Object Collection loaded %s"), *SlotMessage);

		//Save Report on load post serialization from LoadBinaryArchive() -> ProcessObjectCollection()
		if (FSettingHelpers::IsSaveReportEnabled())
		{
			for (const FRawObjectSaveData& Entry : Objects)
			{
				if (Entry.IsValidData())
				{
					WriteSaveReport(FSaveReportContext::ForCollection(Entry.Object, FileName, bUseSlot, true));
				}
			}
		}
	}

	return bLoadBinary;
}

bool UEMSObjectAdv::ProcessObjectCollection(FMemoryReader& FromBinary, const FSaveObjects& Objects) const
{
	//Deserialze as same type
	TMap<FString, TArray<uint8>> DataWithId;
	FromBinary << DataWithId;

	if (DataWithId.IsEmpty())
	{
		return false;
	}

	bool bAnyLoaded = false;
	for (const FRawObjectSaveData& Entry : Objects.GetRawData())
	{
		if (!Entry.IsValidData())
		{
			continue;
		}

		//Find the Object by Id
		const TArray<uint8>* Binary = DataWithId.Find(Entry.Id);
		if (!Binary || Binary->IsEmpty())
		{
			continue;
		}

		FStructHelpers::SerializeStruct(Entry.Object);
		SerializeFromBinary(Entry.Object, *Binary, FSerializeMeta::Collection());

		bAnyLoaded = true;
	}

	return bAnyLoaded;
}

FString UEMSObjectAdv::GetObjectCollectionPath(const bool bUseSlot, const FString& CollectionFileName) const
{
	const bool bForceDefaultName = 
		EMS::StringEmpty(CollectionFileName) || 
		IsFileNameReserved(CollectionFileName, true);

	const FString FileName = bForceDefaultName ? EMS::ObjectCollection : CollectionFileName;
	const FString SlotName = bUseSlot ? GetCurrentSaveGameName() : FString();
	const FString FullPath = CustomSaveFile(FileName, SlotName);

	return FullPath;
}

/**
Shared Cloud and Save Slot Copy helpers.
Other helpers are in the sections of the individual implementations.
**/

namespace
{
	static void InvalidateSlotMutation(UEMSObject* EMSMain, const FString& TargetSlot, const bool bTouchedCoreSlotData,
		const TArray<FString>& CustomFiles, const bool bGlobalCustom = false)
	{
		if (bTouchedCoreSlotData)
		{
			if (TargetSlot.Equals(EMSMain->GetCurrentSaveGameName(), ESearchCase::IgnoreCase))
			{
				EMSMain->ClearUserData();
			}
			EMSMain->ClearCachedSaveSlot(TargetSlot);
		}

		const FString CustomSlot = bGlobalCustom ? FString() : TargetSlot;
		for (const FString& CustomName : CustomFiles)
		{
			EMSMain->ClearCachedCustomSave(CustomSlot, CustomName);
		}
	}

	static bool NormalizeTargetSlotInfo(UEMSObject* EMSMain, const FString& TargetSlot, const TCHAR* Operation)
	{
		EMSMain->ClearCachedSaveSlot(TargetSlot);
		UEMSInfoSaveGame* TargetSlotInfo = EMSMain->GetSlotInfoObject(TargetSlot);
		if (!TargetSlotInfo || EMS::StringEmpty(TargetSlotInfo->SlotInfo.Name))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("%s: Failed to load Target Slot Info: %s"), Operation, *TargetSlot);
			return false;
		}

		if (!EMSMain->StampSlotInfo(TargetSlot, Operation))
		{
			return false;
		}

		EMSMain->ClearCachedSaveSlot(TargetSlot);

		TargetSlotInfo = EMSMain->GetSlotInfoObject(TargetSlot);
		if (!TargetSlotInfo || !TargetSlotInfo->SlotInfo.Name.Equals(TargetSlot, ESearchCase::CaseSensitive))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("%s: Failed to normalize Target Slot Info: %s"), Operation, *TargetSlot);
			return false;
		}

		return true;
	}
}

/**
Save Slot Copy
Copies platform paths. Desktop folders and console files behave identically.
**/

bool UEMSObjectAdv::TransferCustom(TSubclassOf<UEMSCustomSaveGame> SaveGameClass,
	const FString& TargetSaveSlot, FString SourceSaveSlot, FString FileName)
{
	if (!SaveGameClass)
	{
		return false;
	}

	if (UEMSObject* EMSMain = UEMSObject::Get(this))
	{
		if (UEMSCustomSaveGame* SaveGame = EMSMain->GetCustomSave(SaveGameClass, SourceSaveSlot, FileName, false))
		{
			return EMSMain->TransferCustom(SaveGame, TargetSaveSlot);
		}
	}

	return false;
}

namespace
{
	struct FSlotCopyItem
	{
		FSlotCopyItem() = default;

		FSlotCopyItem(const FString& InSourcePath, const FString& InTargetPath)
			: SourcePath(InSourcePath)
			, TargetPath(InTargetPath)
		{
		}

		FString SourcePath;
		FString TargetPath;
		TArray<uint8> Data;
		FString CustomName;
		bool bSourceExists = false;
		bool bThumbnail = false;
		bool bCustom = false;
	};
}

bool UEMSObjectAdv::CopySaveSlot(const FString& TargetSlot, const FString& SourceSlot, ECopySaveSlotType CopyType, const TArray<FString>& CustomFiles, bool bOverwriteExisting)
{
	//Checks the task objects directly, so copying also works while the game is paused.
	if (IsAsyncTaskActive())
	{
		return false;
	}

	if (EMS::StringEmpty(TargetSlot))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Target Slot is empty"));
		return false;
	}

	const bool bCopyFullSlot = CopyType == ECopySaveSlotType::FullSlot;
	if (!bCopyFullSlot && CustomFiles.IsEmpty())
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Custom Files Only requires at least one Custom file"));
		return false;
	}

	UEMSObject* EMSMain = UEMSObject::Get(this);

	//Both resolve to a real name. ValidateSaveName falls back rather than returning empty.
	const FString ValidSource = EMS::StringEmpty(SourceSlot) ? GetCurrentSaveGameName() : ValidateSaveName(SourceSlot);
	const FString ValidTarget = ValidateSaveName(TargetSlot);

	if (ValidSource.Equals(ValidTarget, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Source and Target Slot are the same"));
		return false;
	}

	//Same rules as the Current Slot, so a copy cannot write onto an internal EMS path
	if (FSavePaths::IsSlotNameReserved(ValidSource) || FSavePaths::IsSlotNameReserved(ValidTarget))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Reserved Slot name rejected"));
		return false;
	}

	//Default files for full slot
	TArray<FSlotCopyItem> CopyPlan;
	CopyPlan.Reserve((bCopyFullSlot ? 4 : 0) + CustomFiles.Num());
	if (bCopyFullSlot)
	{
		CopyPlan.Add({ PlayerSaveFile(ValidSource), PlayerSaveFile(ValidTarget) });
		CopyPlan.Add({ ActorSaveFile(ValidSource), ActorSaveFile(ValidTarget) });
		CopyPlan.Add({ SlotInfoSaveFile(ValidSource), SlotInfoSaveFile(ValidTarget) });

		FSlotCopyItem& Thumbnail = CopyPlan.AddDefaulted_GetRef();
		Thumbnail.SourcePath = ThumbnailSaveFile(ValidSource);
		Thumbnail.TargetPath = ThumbnailSaveFile(ValidTarget);
		Thumbnail.bThumbnail = true;
	}

	//Optional custom files
	TSet<FString> CustomDestinations;
	TArray<FString> TouchedCustomNames;
	TouchedCustomNames.Reserve(CustomFiles.Num());

	for (const FString& CustomFile : CustomFiles)
	{
		//Same as the cloud export, an unusable name only skips itself and never fails the whole copy
		if (EMS::StringEmpty(CustomFile))
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Custom file name is empty and was skipped"));
			continue;
		}

		const FString ValidCustomName = ValidateSaveName(CustomFile);
		if (IsFileNameReserved(ValidCustomName))
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Custom file '%s' is reserved and was skipped"), *CustomFile);
			continue;
		}

		const FString SourcePath = CustomSaveFile(ValidCustomName, ValidSource);
		const FString TargetPath = CustomSaveFile(ValidCustomName, ValidTarget);
		const FString CanonicalDestination = TargetPath.ToLower();

		if (CustomDestinations.Contains(CanonicalDestination))
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Custom file '%s' resolves to a duplicate destination and was skipped"), *CustomFile);
			continue;
		}

		CustomDestinations.Add(CanonicalDestination);

		FSlotCopyItem& Item = CopyPlan.AddDefaulted_GetRef();
		Item.SourcePath = SourcePath;
		Item.TargetPath = TargetPath;
		Item.CustomName = ValidCustomName;
		Item.bCustom = true;
	}

	//A complete copy requires a real source Slot. 
	if (bCopyFullSlot && !FSaveFileIntegrity::DoesSaveExist(FFileContext(PlayerIndex, SlotInfoSaveFile(ValidSource))))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Source Slot does not exist: %s"), *ValidSource);
		return false;
	}

	//Preflight before any target change. Missing core files clone as missing, a missing Custom file skips itself.
	int32 CopiedCustomCount = 0;
	for (FSlotCopyItem& Item : CopyPlan)
	{
		const FFileContext SourceContext(PlayerIndex, Item.SourcePath);
		Item.bSourceExists = FSaveFileIntegrity::DoesSaveExist(SourceContext);

		if (!Item.bSourceExists)
		{
			if (Item.bCustom)
			{
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Custom source file was skipped: %s"), *Item.SourcePath);
			}

			continue;
		}

		if (FSaveFileIntegrity::LoadFromFile(SourceContext, Item.Data) != EFileValidity::FILE_VALID)
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Copy Save Slot: Failed to read source file: %s"), *Item.SourcePath);
			return false;
		}

		//A copy is a byte transfer, so Legacy is accepted here. The Thumbnail is a plain image without a header.
		if (!Item.bThumbnail && !FSaveFileIntegrity::VerifyLocalFile(Item.SourcePath, Item.Data))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Copy Save Slot: Source file is corrupt: %s"), *Item.SourcePath);
			return false;
		}

		if (Item.bCustom)
		{
			++CopiedCustomCount;
		}
	}

	//Custom Files Only has nothing left to do when none of the requested files exist.
	if (!bCopyFullSlot && CopiedCustomCount == 0)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: None of the requested Custom files exist"));
		return false;
	}

	//Conflicts are checked before any target mutation. A skipped Custom file is never written, so it cannot conflict.
	if (!bOverwriteExisting)
	{
		for (const FSlotCopyItem& Item : CopyPlan)
		{
			if (!Item.bSourceExists && Item.bCustom)
			{
				continue;
			}

			if (FSaveFileIntegrity::DoesSaveExist(FFileContext(PlayerIndex, Item.TargetPath)))
			{
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Copy Save Slot: Target already exists and overwrite is disabled: %s"), *ValidTarget);
				return false;
			}
		}
	}

	auto InvalidateTargetCaches = [&]()
	{
		InvalidateSlotMutation(EMSMain, ValidTarget, bCopyFullSlot, TouchedCustomNames);
	};

	for (const FSlotCopyItem& Item : CopyPlan)
	{
		const FFileContext TargetContext(PlayerIndex, Item.TargetPath);
		if (Item.bSourceExists)
		{
			//A failed protected write can still leave the destination uncertain, so invalidate only after this target is reached.
			if (Item.bCustom)
			{
				TouchedCustomNames.Add(Item.CustomName);
			}

			const bool bWritten = Item.bThumbnail
				? FSaveFileIntegrity::SaveThumbnailToFile(TargetContext, Item.Data)
				: FSaveFileIntegrity::SaveToFile(TargetContext, Item.Data);

			if (!bWritten)
			{
				InvalidateTargetCaches();
				UE_LOG(LogEasyMultiSave, Error, TEXT("Copy Save Slot: Failed to write target file: %s"), *Item.TargetPath);
				return false;
			}
		}
		else if (!Item.bCustom && FSaveFileIntegrity::DoesSaveExist(TargetContext))
		{
			if (!FSaveFileIntegrity::DeleteSaveFile(TargetContext))
			{
				InvalidateTargetCaches();
				UE_LOG(LogEasyMultiSave, Error, TEXT("Copy Save Slot: Failed to remove stale target file: %s"), *Item.TargetPath);
				return false;
			}
		}
	}

	//Discard the active multi-level state first, or GetSlotInfoObject merges old levels into the copied Slot Info.
	if (bCopyFullSlot && ValidTarget.Equals(EMSMain->GetCurrentSaveGameName(), ESearchCase::IgnoreCase))
	{
		EMSMain->ClearUserData();
	}

	if (bCopyFullSlot)
	{
		//The raw Slot Info still holds the Source identity, so reopen it through EMS and verify the normalized result.
		if (!NormalizeTargetSlotInfo(EMSMain, ValidTarget, TEXT("Copy Save Slot")))
		{
			InvalidateTargetCaches();
			return false;
		}
	}
	else if (!EMSMain->StampSlotInfo(ValidTarget, TEXT("Copy Save Slot")))
	{
		InvalidateTargetCaches();
		return false;
	}

	//Copied primaries start a new Target history, including any backup that normalization just created.
	for (const FSlotCopyItem& Item : CopyPlan)
	{
		//A skipped Custom file left its primary alone, so its history stays valid.
		if (Item.bThumbnail || (Item.bCustom && !Item.bSourceExists))
		{
			continue;
		}

		if (!RemoveStaleBackup(Item.TargetPath, TEXT("Copy Save Slot")))
		{
			InvalidateTargetCaches();
			return false;
		}
	}

	//Core data was already invalidated, but a cached Custom object would still overwrite the copy on its next save.
	InvalidateSlotMutation(EMSMain, ValidTarget, false, TouchedCustomNames);

	UE_LOG(LogEasyMultiSave, Log, TEXT("Save Slot copied from %s to %s"), *ValidSource, *ValidTarget);
	return true;
}

/**
Native Cloud Saving
Required for EOS and several other platforms
**/

namespace
{
	struct FCloudImportItem
	{
		FString FilePath;
		const TArray<uint8>* Data = nullptr;
		bool bIsSlotInfo = false;
	};

	static bool IsBlueprintCloudType(const ECloudFileType FileType)
	{
		return (FileType == ECloudFileType::FullSlot || FileType == ECloudFileType::Custom || FileType == ECloudFileType::CustomSlot);
	}

	//Global Custom resolves outside any Slot, so its Slot argument is meaningless and never validated.
	static bool IsSlotBoundCloudType(const ECloudFileType FileType)
	{
		return FileType != ECloudFileType::Custom;
	}
}

bool UEMSObjectAdv::ExportCloudData(ECloudFileType FileType, const FString& SourceSlot, const TArray<FString>& CustomFiles, TArray<uint8>& ByteData)
{
	//Checks the task objects directly, so cloud IO also works while the game is paused
	if (IsAsyncTaskActive())
	{
		return false;
	}

	ByteData.Empty();
	const FString ValidSource = EMS::StringEmpty(SourceSlot) ? GetCurrentSaveGameName() : ValidateSaveName(SourceSlot);
	if (IsSlotBoundCloudType(FileType) && FSavePaths::IsSlotNameReserved(ValidSource))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Source Slot is invalid"));
		return false;
	}

	//Handle Manifest Packing for Multiple Files
	if (IsBlueprintCloudType(FileType))
	{
		TArray<FCloudArchiveEntry> Entries;

		//Pack Core Files if FullSlot. The Thumbnail is deliberately not part of a cloud Slot.
		if (FileType == ECloudFileType::FullSlot)
		{
			struct FSlotFile
			{
				const FString& Key;
				ECloudFileType Type;
			};

			const TArray<FSlotFile> CoreFiles =
			{
				{ EMS::PlayerSuffix, ECloudFileType::Player   },
				{ EMS::ActorSuffix,  ECloudFileType::Level    },
				{ EMS::SlotSuffix,   ECloudFileType::SlotInfo }
			};

			for (const FSlotFile& File : CoreFiles)
			{
				const FString Path = GetCloudFilePath(FCloudFilePath(File.Type, FString()), ValidSource);

				FCloudArchiveEntry Entry;
				Entry.FileKey = File.Key;

				const EFileValidity Validity = ReadDiskFile(Path, Entry.Data);
				if (Validity == EFileValidity::FILE_VALID)
				{
					Entries.Add(MoveTemp(Entry));
					continue;
				}

				//A file that exists but cannot be read is never quietly dropped from a complete Slot.
				if (Validity != EFileValidity::FILE_MISSING)
				{
					UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Source Slot file '%s' is corrupt"), *File.Key);
					return false;
				}

				//Player and Level are optional. A complete Slot is defined by its Slot Info, same as Copy Save Slot.
				if (File.Type == ECloudFileType::SlotInfo)
				{
					UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Source Slot does not exist: %s"), *ValidSource);
					return false;
				}
			}
		}

		//If FullSlot, assume extra files are in the slot. Otherwise use the specified Custom type.
		const ECloudFileType CustomOverride = (FileType == ECloudFileType::FullSlot) ? ECloudFileType::CustomSlot : FileType;
		TSet<FString> PackedDestinations;
		for (const FString& CustomName : CustomFiles)
		{
			//An unusable name only skips itself, the rest of the export is still packed
			if (EMS::StringEmpty(CustomName))
			{
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Custom file name is empty and was skipped"));
				continue;
			}

			//Skip names that collide with core slot file keys (Player/Level/Slot)
			if (IsFileNameReserved(CustomName, true))
			{
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Custom file name '%s' is reserved and was skipped"), *CustomName);
				continue;
			}

			const FString Path = GetCloudFilePath(FCloudFilePath(CustomOverride, CustomName), ValidSource);

			//Skip names that resolve to an already packed file
			if (PackedDestinations.Contains(Path.ToLower()))
			{
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Custom file name '%s' resolves to an already packed file and was skipped"), *CustomName);
				continue;
			}

			//Reserved before the read, so a duplicate stays a duplicate even if the file is missing.
			PackedDestinations.Add(Path.ToLower());

			//The packed key is the resolved file name, so the data never carries raw input.
			FCloudArchiveEntry Entry;
			Entry.FileKey = ValidateSaveName(CustomName);

			const EFileValidity Validity = ReadDiskFile(Path, Entry.Data);
			if (Validity != EFileValidity::FILE_VALID)
			{
				//Only true absence is tolerated. A broken file would silently ship as missing data.
				if (Validity != EFileValidity::FILE_MISSING)
				{
					UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Custom source file '%s' is corrupt"), *CustomName);
					return false;
				}

				//A missing Custom file only skips itself. The rest of the export is still packed.
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Custom source file was skipped: %s"), *CustomName);
				continue;
			}

			Entries.Add(MoveTemp(Entry));
		}

		if (Entries.IsEmpty())
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Nothing to export for Slot: %s"), *ValidSource);
			return false;
		}

		//Write packed manifest to output
		FMemoryWriter Writer(ByteData, true);
		Writer << Entries;

		return true;
	}

	//Handle Single Core File Export (Player, Level, SlotInfo)
	const FString FilePath = GetCloudFilePath(FCloudFilePath(FileType, FString()), ValidSource);

	return ReadDiskFile(FilePath, ByteData) == EFileValidity::FILE_VALID;
}

bool UEMSObjectAdv::ImportCloudData(const TArray<uint8>& ByteData, ECloudFileType FileType, const FString& TargetSlot)
{
	//Checks the task objects directly, so cloud IO also works while the game is paused
	if (IsAsyncTaskActive())
	{
		return false;
	}

	if (ByteData.IsEmpty())
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Import data is empty"));
		return false;
	}

	const FString ValidTarget = EMS::StringEmpty(TargetSlot) ? GetCurrentSaveGameName() : ValidateSaveName(TargetSlot);
	if (IsSlotBoundCloudType(FileType) && FSavePaths::IsSlotNameReserved(ValidTarget))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Target Slot is invalid"));
		return false;
	}

	UEMSObject* EMSMain = UEMSObject::Get(this);

	const bool bManifestImport = IsBlueprintCloudType(FileType);

	TArray<FCloudArchiveEntry> Entries;
	TArray<FCloudImportItem> ImportPlan;
	TArray<FString> ImportedCustomFiles;
	TSet<FString> Destinations;

	bool bTouchedCoreSlotData = false;
	bool bImportedSlotInfo = false;
	const bool bGlobalCustom = FileType == ECloudFileType::Custom;

	//Manifest and single-file imports build the same write plan, then share mutation finalization.
	if (bManifestImport)
	{
		FMemoryReader Reader(ByteData, true);
		Reader << Entries;

		if (Reader.IsError() || Reader.IsCriticalError() || Entries.IsEmpty())
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Failed to parse manifest blob"));
			return false;
		}

		ImportPlan.Reserve(Entries.Num());

		//Validate every entry before mutating disk, so a malformed later entry cannot half-apply the import.
		for (const FCloudArchiveEntry& Entry : Entries)
		{
			//An empty key would resolve onto the Slot folder itself. An empty payload is caught by the integrity check.
			if (Entry.FileKey.IsEmpty())
			{
				UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Manifest contains an entry with an empty key"));
				return false;
			}

			ECloudFileType EntryType = FileType;
			bool bIsCoreFile = false;

			//If FullSlot, we must dynamically figure out if this entry is a core file or a custom slot file.
			if (FileType == ECloudFileType::FullSlot)
			{
				if (Entry.FileKey == EMS::PlayerSuffix)
				{
					EntryType = ECloudFileType::Player;
					bIsCoreFile = true;
				}
				else if (Entry.FileKey == EMS::ActorSuffix)
				{
					EntryType = ECloudFileType::Level;
					bIsCoreFile = true;
				}
				else if (Entry.FileKey == EMS::SlotSuffix)
				{
					EntryType = ECloudFileType::SlotInfo;
					bIsCoreFile = true;
				}
				else
				{
					EntryType = ECloudFileType::CustomSlot;
				}
			}

			if (!bIsCoreFile && IsFileNameReserved(Entry.FileKey, true))
			{
				UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Reserved custom file key '%s' in manifest"), *Entry.FileKey);
				return false;
			}

			const FString Path = GetCloudFilePath(FCloudFilePath(EntryType, Entry.FileKey), ValidTarget);

			//Save paths are case-insensitive on the desktop targets supported by the plugin.
			const FString CanonicalPath = Path.ToLower();
			if (Destinations.Contains(CanonicalPath))
			{
				UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Manifest contains duplicate destination '%s'"), *Path);
				return false;
			}

			if (!FSaveFileIntegrity::VerifyCloudFile(Path, Entry.Data))
			{
				UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Integrity check failed for manifest entry '%s'"), *Entry.FileKey);
				return false;
			}

			Destinations.Add(CanonicalPath);
			ImportPlan.Add({ Path, &Entry.Data, EntryType == ECloudFileType::SlotInfo });
			if (!bIsCoreFile)
			{
				ImportedCustomFiles.AddUnique(ValidateSaveName(Entry.FileKey));
			}
			bTouchedCoreSlotData |= bIsCoreFile;
			bImportedSlotInfo |= EntryType == ECloudFileType::SlotInfo;
		}

		//A complete Slot is defined by its Slot Info, same as Copy Save Slot.
		if (FileType == ECloudFileType::FullSlot && !bImportedSlotInfo)
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Complete Slot data has no Slot Info"));
			return false;
		}
	}
	else
	{
		const FString FilePath = GetCloudFilePath(FCloudFilePath(FileType, FString()), ValidTarget);

		//Single-file imports use the same preflight integrity check as Manifest entries.
		if (!FSaveFileIntegrity::VerifyCloudFile(FilePath, ByteData))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Integrity check failed! Aborting write to '%s'"), *FilePath);
			return false;
		}

		ImportPlan.Add({ FilePath, &ByteData, FileType == ECloudFileType::SlotInfo });
		bTouchedCoreSlotData = true;
		bImportedSlotInfo = FileType == ECloudFileType::SlotInfo;
	}

	auto InvalidateTargetCaches = [&]()
	{
		InvalidateSlotMutation(EMSMain, ValidTarget, bTouchedCoreSlotData, ImportedCustomFiles, bGlobalCustom);
	};

	bool bWroteSlotInfo = false;
	for (const FCloudImportItem& Item : ImportPlan)
	{
		if (!WriteDiskFile(Item.FilePath, *Item.Data))
		{
			//A manifest may already have written earlier entries. Direct imports preserve their old failure behavior.
			if (bManifestImport)
			{
				InvalidateTargetCaches();

				//An imported Slot Info still carries the Source identity until it is normalized
				if (bWroteSlotInfo)
				{
					NormalizeTargetSlotInfo(EMSMain, ValidTarget, TEXT("Cloud"));
				}
			}
			return false;
		}

		bWroteSlotInfo |= Item.bIsSlotInfo;
	}

	//A complete Slot replaces the core Slot, so files the data did not carry are cloned as missing.
	if (FileType == ECloudFileType::FullSlot)
	{
		for (const ECloudFileType CoreType : { ECloudFileType::Player, ECloudFileType::Level })
		{
			const FString CorePath = GetCloudFilePath(FCloudFilePath(CoreType, FString()), ValidTarget);
			const FFileContext CoreContext(PlayerIndex, CorePath);

			if (Destinations.Contains(CorePath.ToLower()) || !FSaveFileIntegrity::DoesSaveExist(CoreContext))
			{
				continue;
			}

			if (!FSaveFileIntegrity::DeleteSaveFile(CoreContext))
			{
				InvalidateTargetCaches();
				UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Failed to remove stale target file: %s"), *CorePath);
				return false;
			}

			//The primary is gone, so its backup would be the only remaining copy of the old data.
			if (!RemoveStaleBackup(CorePath, TEXT("Cloud")))
			{
				InvalidateTargetCaches();
				return false;
			}

		}
	}

	InvalidateTargetCaches();

	if (bImportedSlotInfo)
	{
		if (!NormalizeTargetSlotInfo(EMSMain, ValidTarget, TEXT("Cloud")))
		{
			InvalidateTargetCaches();
			return false;
		}
	}
	//Slot-bound Custom files live inside the Target Slot, which is only listed while it has a Slot Info.
	else if (IsSlotBoundCloudType(FileType) && !ImportedCustomFiles.IsEmpty())
	{
		if (!EMSMain->StampSlotInfo(ValidTarget, TEXT("Cloud")))
		{
			InvalidateTargetCaches();
			return false;
		}
	}

	for (const FCloudImportItem& Item : ImportPlan)
	{
		if (!RemoveStaleBackup(Item.FilePath, TEXT("Cloud")))
		{
			InvalidateTargetCaches();
			return false;
		}
	}

	return true;
}

EFileValidity UEMSObjectAdv::ReadDiskFile(const FString& FilePath, TArray<uint8>& OutData) const
{
	//LoadFromFile already separates missing from unreadable, so no extra existence check is needed.
	EFileValidity Result = FSaveFileIntegrity::LoadFromFile(FFileContext(PlayerIndex, FilePath), OutData);

	//LoadFromFile does not hash-check the payload, so apply the same gate the import side uses.
	if (Result == EFileValidity::FILE_VALID && !FSaveFileIntegrity::VerifyCloudFile(FilePath, OutData))
	{
		Result = EFileValidity::FILE_INVALID;
	}

	if (Result != EFileValidity::FILE_VALID)
	{
		//Never hand back a payload the caller must remember not to trust
		OutData.Empty();
	}

	if (Result == EFileValidity::FILE_MISSING)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Cloud: Save File does not exist '%s'"), *FilePath);
	}
	else if (Result != EFileValidity::FILE_VALID)
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Corrupt or unreadable data at '%s'"), *FilePath);
	}
	else
	{
		UE_LOG(LogEasyMultiSave, Log, TEXT("Cloud: Exported %d bytes for cloud save '%s'"), OutData.Num(), *FilePath);
	}

	return Result;
}

bool UEMSObjectAdv::WriteDiskFile(const FString& FilePath, const TArray<uint8>& InData)
{
	const FFileContext Context(PlayerIndex, FilePath);
	if (!FSaveFileIntegrity::SaveToFile(Context, InData))
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("Cloud: Failed to write file '%s'"), *FilePath);
		return false;
	}

	UE_LOG(LogEasyMultiSave, Log, TEXT("Cloud: Imported %d bytes to disk '%s'"), InData.Num(), *FilePath);
	return true;
}

FString UEMSObjectAdv::GetCloudFilePath(const FCloudFilePath& Path, const FString& SlotName) const
{
	switch (Path.Type)
	{
	case ECloudFileType::Player:
		return PlayerSaveFile(SlotName);

	case ECloudFileType::Level:
		return ActorSaveFile(SlotName);

	case ECloudFileType::SlotInfo:
		return SlotInfoSaveFile(SlotName);

	case ECloudFileType::Custom:
		return CustomSaveFile(Path.FileName, FString());

	case ECloudFileType::CustomSlot:
		return CustomSaveFile(Path.FileName, SlotName);

	default:
		return FString();
	}
}

/**
Save Slot and User
Must use the one from the main system, otherwise it uses the default Slot
**/

FString UEMSObjectAdv::GetCurrentSaveGameName() const
{
	if (const UEMSObject* EMSMain = UEMSObject::Get(this))
	{
		return EMSMain->GetCurrentSaveGameName();
	}
	return Super::GetCurrentSaveGameName();
}

FString UEMSObjectAdv::GetCurrentSaveUserName() const
{
	if (const UEMSObject* EMSMain = UEMSObject::Get(this))
	{
		return EMSMain->GetCurrentSaveUserName();
	}
	return Super::GetCurrentSaveUserName();
}

