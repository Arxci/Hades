//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSPaths.h"
#include "EMSMisc.h"
#include "EMSData.h"
#include "EMSPluginSettings.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/BufferArchive.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ImageUtils.h"
#include "ImageCore.h"

/**
FSavePaths
**/

TArray<FString> FSavePaths::GetDefaultSaveFiles(const FString& SaveGameName)
{
	TArray<FString> AllFiles;

	using namespace EMS;
	{
		const FString PlayerFile = SaveGameName + UnderscoreFile + PlayerSuffix;
		const FString LevelFile = SaveGameName + UnderscoreFile + ActorSuffix;
		const FString SlotFile = SaveGameName + UnderscoreFile + SlotSuffix;
		const FString ThumbFile = SaveGameName + UnderscoreFile + ThumbSuffix;

		AllFiles.Add(PlayerFile);
		AllFiles.Add(LevelFile);
		AllFiles.Add(SlotFile);
		AllFiles.Add(ThumbFile);
	}

	return AllFiles;
}

FString FSavePaths::GetBackupSavePath(const FString& FullSavePath)
{
	FString SavePath = FullSavePath;

	//Uses prefix
	if (FSettingHelpers::IsConsoleFileSystem())
	{
		const FString BackupTag = EMS::BackupTag + EMS::UnderscoreFile;
		SavePath = BackupTag + FullSavePath;

		return SavePath;
	}

	//Uses suffix, find last slash(folder)
	const int32 SlashIndex = SavePath.Find(EMS::Slash, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (SlashIndex != INDEX_NONE)
	{
		const FString BackupTag = EMS::UnderscoreFile + EMS::BackupTag;
		const FString Directory = SavePath.Left(SlashIndex) + BackupTag;
		const FString FileName = SavePath.Mid(SlashIndex + 1);
		SavePath = Directory + EMS::Slash + FileName;

		return SavePath;
	}

	//Same dir, just append tag
	SavePath = FullSavePath + EMS::UnderscoreFile + EMS::BackupTag;
	
	return SavePath;
}

FString FSavePaths::GetBackupFileName(const FString& BaseName)
{
	if (FSettingHelpers::IsConsoleFileSystem())
	{
		return EMS::BackupTag + EMS::UnderscoreFile + BaseName;
	}

	return BaseName + EMS::UnderscoreFile + EMS::BackupTag;
}

FString FSavePaths::GetBaseSavePath(const FString& FullSavePath)
{	
	//This mirrors the path from ISaveSystem
	return FPaths::ProjectSavedDir() + EMS::SaveGamesFolder + FullSavePath + EMS::SaveType;
}

TArray<FString> FSavePaths::GetSortedSaveSlots(TArray<FSaveSlotInfo>& SaveSlots)
{
	SaveSlots.Sort([](const FSaveSlotInfo& A, const FSaveSlotInfo& B)
	{
		return A.TimeStamp > B.TimeStamp;
	});

	TArray<FString> SaveSlotNames;
	for (const FSaveSlotInfo& SlotInfo : SaveSlots)
	{
		//Skip backup files/folders and the user dir
		if (IsSlotNameReserved(SlotInfo.Name))
		{
			continue;
		}

		SaveSlotNames.Add(SlotInfo.Name);
	}

	return SaveSlotNames;
}

static const TArray<FString> GetDefaultSuffixes() 
{
	static const TArray<FString> DefaultSuffixes =
	{
		EMS::PlayerSuffix,
		EMS::ActorSuffix,
		EMS::SlotSuffix,
		EMS::ThumbSuffix
	};

	return DefaultSuffixes;
};

bool FSavePaths::IsFileNameReserved(const FString& SaveGameName)
{
	for (const FString& Reserved : GetDefaultSuffixes())
	{
		if (SaveGameName.Equals(Reserved, ESearchCase::IgnoreCase))
		{
			UE_LOG(LogEasyMultiSave, Error, TEXT("Save name '%s' is reserved and its recommended to change it into something else!"), *SaveGameName);
			return true;
		}
	}

	return false;
}

bool FSavePaths::IsSlotNameReserved(const FString& SlotName)
{
	/*
	These names belong to the User folder and the backup scheme, a Slot must never resolve onto them.
	Static because this runs on every resolved Save path through GetCurrentSaveGameName
	*/
	static const FString BakSuffix = EMS::UnderscoreFile + EMS::BackupTag;
	static const FString BakPrefix = EMS::BackupTag + EMS::UnderscoreFile;

	return SlotName.Equals(EMS::UserDir, ESearchCase::IgnoreCase)
		|| SlotName.Equals(EMS::BackupTag, ESearchCase::IgnoreCase)
		|| SlotName.EndsWith(BakSuffix, ESearchCase::IgnoreCase)
		|| SlotName.StartsWith(BakPrefix, ESearchCase::IgnoreCase);
}

FString FSavePaths::ValidateSaveName(const FString& SaveGameName)
{
	FString CurrentSave = SaveGameName;
	CurrentSave.TrimStartAndEndInline();

	//Legitimately empty input must stay empty
	if (CurrentSave.IsEmpty())
	{
		return FString();
	}

	//Strip path separators up-front
	CurrentSave.ReplaceInline(TEXT("/"), *EMS::UnderscoreFile);
	CurrentSave.ReplaceInline(TEXT("\\"), *EMS::UnderscoreFile);

	//Remove default suffixes
	if (FSettingHelpers::IsConsoleFileSystem())
	{
		
		/*
		Sort of idempotent, because Level(Actor), Player, Slot, Thumb
		Suffixes are the LAST thing added before the path is used to write the file
		See FullSaveDir
		*/
		for (const FString& Suffix : GetDefaultSuffixes())
		{
			if (CurrentSave.EndsWith(Suffix, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogEasyMultiSave, Warning, TEXT("Save name '%s' contains reserved suffix '%s' "), *SaveGameName, *Suffix);
				CurrentSave.RemoveFromEnd(*Suffix);
			}
		}

		CurrentSave.TrimEndInline();

		//Replaces _ with alnum 
		CurrentSave.ReplaceInline(TEXT("_"), *EMS::UnderscoreFile);
	}

	CurrentSave.ReplaceInline(TEXT(" "), *EMS::UnderscoreFile);
	CurrentSave.ReplaceInline(TEXT("."), *EMS::UnderscoreFile);
	CurrentSave.ReplaceInline(TEXT("-"), *EMS::UnderscoreFile);
	CurrentSave.ReplaceInline(TEXT("@"), *EMS::UnderscoreFile);

	//@TODO On Mac/linux folders are case sensitive, we still need to guard against MySlot and myslot on actual creation. maybe defer for now. 

	const FString Result = FPaths::MakeValidFileName(*CurrentSave);

	//Total guarantee: never an empty path.
	if (Result.IsEmpty())
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Save name '%s' sanitized to empty, using fallback."), *SaveGameName);
		return EMS::FallbackName;
	}

	return Result;
}

FString FSavePaths::GetThumbnailFormat()
{
	if (UEMSPluginSettings::Get()->ThumbnailFormat == EThumbnailImageFormat::Png)
	{
		return EMS::ImgFormatPNG;
	}

	return EMS::ImgFormatJPG;
}

void FSavePaths::CheckForReadOnly(const FString& FullSavePath)
{
	const FString NativePath = FString::Printf(EMS::NativeDesktopSavePath, *FPaths::ProjectSavedDir(), *FullSavePath);
	const bool bReadOnly = IFileManager::Get().IsReadOnly(*NativePath);
	if (bReadOnly)
	{
		FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*NativePath, false);
		UE_LOG(LogEasyMultiSave, Warning, TEXT("File access read only was set to false: %s"), *NativePath);
	}
}

TArray<FString> FSavePaths::GetConsoleSlotFiles(const TArray<FString>& SaveGameNames)
{
	const FString FullSlotSuffix = EMS::UnderscoreFile + EMS::SlotSuffix;

	//Filter out slots
	TArray<FString> SlotNames;
	for (const FString& ActualFileName : SaveGameNames)
	{
		const bool bIsActualSlot = ActualFileName.EndsWith(FullSlotSuffix);
		if (bIsActualSlot)
		{
			//Get actual name without suffix
			const int32 Index = ActualFileName.Find(FullSlotSuffix, ESearchCase::IgnoreCase, ESearchDir::FromEnd, INDEX_NONE);
			const int32 Count = FullSlotSuffix.Len();

			FString ReducedFileName = ActualFileName;
			ReducedFileName.RemoveAt(Index, Count);
			SlotNames.Add(ReducedFileName);
		}
	}

	return SlotNames;
}

bool FSavePaths::IsSafeToDelete(const FString& AbsoluteBaseDir, const FString& InputFolder)
{
	//Reject empty or whitespace-only names
	if (EMS::StringEmpty(InputFolder))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("IsSafeToDelete: input name is empty"));
		return false;
	}

	//Reject names that contain path separators
	if (InputFolder.Contains(TEXT("/")) || InputFolder.Contains(TEXT("\\")) || InputFolder == TEXT(".") || InputFolder == TEXT(".."))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("IsSafeToDelete: input name contains path separator: %s"), *InputFolder);
		return false;
	}

	//Verify resolved path stays inside the save root
	FString Collapsed = FPaths::Combine(AbsoluteBaseDir, InputFolder);
	FPaths::CollapseRelativeDirectories(Collapsed);

	//Explicit make sure it never deletes the root
	if (Collapsed.Equals(AbsoluteBaseDir, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("IsSafeToDelete: path resolves to save root itself: %s"), *Collapsed);
		return false;
	}

	if (!FPaths::IsUnderDirectory(Collapsed, AbsoluteBaseDir))
	{
		UE_LOG(LogEasyMultiSave, Error, TEXT("IsSafeToDelete: path escapes save root: %s"), *Collapsed);
		return false;
	}

	return true;
}

bool FSavePaths::DeleteSaveDirectory(const FString& BaseDir, const FString& InputName, const FString& Context)
{
	//Important safety check
	if (!IsSafeToDelete(BaseDir, InputName))
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("Aborted deletion: Path is not safe to delete for %s: %s"), *Context, *InputName);
		return false;
	}

	const FString FullPath = FPaths::Combine(BaseDir, InputName);
	IFileManager& FileManager = IFileManager::Get();

	//Check if it actually exists
	if (FileManager.DirectoryExists(*FullPath))
	{
		if (FileManager.DeleteDirectory(*FullPath, true, true))
		{
			UE_LOG(LogEasyMultiSave, Log, TEXT("%s data successfully removed for: %s"), *Context, *InputName);
			return true;
		}

		UE_LOG(LogEasyMultiSave, Warning, TEXT("Failed to delete existing %s folder for: %s"), *Context, *InputName);
		return false;
	}

	//It didnt exist, so nothing was deleted
	UE_LOG(LogEasyMultiSave, Log, TEXT("%s data for %s was not found."), *Context, *InputName);

	return false;
}

/**
Thumbnail Functions
**/

bool FSaveThumbnails::CompressImage(const FImage& Image, FArchive& Ar)
{
	TArray64<uint8> CompressedData;

	//The actual compression is CPU-bound and perfectly safe for background threads
	if (!FImageUtils::CompressImage(CompressedData, *FSavePaths::GetThumbnailFormat(), Image, 90))
	{
		return false;
	}

	Ar.Serialize(CompressedData.GetData(), CompressedData.Num());

	return true;
}

/**
FEditorPaths
Slice of Pie
**/

FString FEditorPaths::StripLevelPIEPrefix(const UWorld* InWorld, const FString& LevelName)
{
	if (!InWorld)
	{
		return FString();
	}

	FString FixedLevelName = InWorld->RemovePIEPrefix(LevelName);

	/*
	Support legacy saves that only stripped "PIE_<id>" leaving "UED_" behind
	Thats why this function is not editor only
	*/
	if (FixedLevelName.StartsWith(TEXT("UED_")))
	{
		FixedLevelName.RightChopInline(4);
	}

	return FixedLevelName;
}

#if WITH_EDITOR
void FEditorPaths::StripRefPIEPrefix(FString& PathString)
{
	//Improves compatibility between editor and packaged saves

	if (!FPackageName::GetLongPackageAssetName(PathString).StartsWith(PLAYWORLD_PACKAGE_PREFIX))
	{
		return;
	}

	// UEDPIE_ 
	static const FString PIEHandle = FString::Printf(TEXT("%s_"), PLAYWORLD_PACKAGE_PREFIX);

	if (!PathString.Contains(*PIEHandle, ESearchCase::CaseSensitive))
	{
		return;
	}

	int32 PieId = 0;

#if EMS_ENGINE_MIN_UE55
	PieId = UE::GetPlayInEditorID(); //May be -1 with MT Saving when on a worker thread
#endif

	//Best effort: strip with the reported id (common case)
	const FString PIEPrefix = FString::Printf(TEXT("%s%d_"), *PIEHandle, PieId);
	const int32 BeforeLen = PathString.Len();

	PathString.ReplaceInline(*PIEPrefix, TEXT(""), ESearchCase::CaseSensitive);

	//If unchanged and still contains a PIE marker, strip practical fallbacks
	if (PathString.Len() == BeforeLen && PathString.Contains(PIEHandle, ESearchCase::CaseSensitive))
	{
		/*
		Fallback cleanup for common PIE instance prefixes
		Covers typical multiplayer testing setups (up to 5 PIE worlds)
		*/
		for (int32 PIEIndex = -2; PIEIndex <= 4; ++PIEIndex)
		{
			PathString.ReplaceInline(
				*FString::Printf(TEXT("%s%d_"), *PIEHandle, PIEIndex),
				TEXT(""),
				ESearchCase::CaseSensitive);
		}
	}
}
#endif

/**
FRedirectHelpers
**/

template<typename MapType>
inline FString FRedirectHelpers::ResolveRedirect(const FString& Input, const MapType& RedirectMap)
{
	if (Input.IsEmpty() || RedirectMap.IsEmpty())
	{
		return Input;
	}

	if (const FString* Lookup = RedirectMap.Find(Input))
	{
		return *Lookup;
	}

	/*
	Returns input unchanged if no redirect exists.

	Redirects map OLD level names > NEW names when loading save data.
	After re-saving, references use the NEW names (natural migration).

	This allows renaming levels while maintaining backwards compatibility with old saves.
	*/

	return Input;
}

FString FRedirectHelpers::ResolveWorldLevel(const FString& LevelPath)
{
	//Reversed paths
	const TMap<FString, FString>& Redirectors = UEMSPluginSettings::Get()->ReverseLevelRedirects;
	if (Redirectors.IsEmpty())
	{
		return LevelPath;
	}

	const FString FinalName = ResolveRedirect(LevelPath, Redirectors);
	//UE_LOG(LogEasyMultiSave, Warning, TEXT(" ResolveWorldLevel  %s"), *FinalName);
	return FinalName;
}

FString FRedirectHelpers::ResolveActorLevel(const FString& ActorLevelName)
{
	//Reversed short names
	const TMap<FString, FString>& Redirectors = UEMSPluginSettings::Get()->ReverseShortLevelRedirects;
	if (Redirectors.IsEmpty())
	{
		return ActorLevelName;
	}

	const FString FinalName = ResolveRedirect(ActorLevelName, Redirectors);
	//UE_LOG(LogEasyMultiSave, Warning, TEXT(" ResolveActorLevel  %s"), *FinalName);
	return FinalName;
}

FSoftObjectPath FRedirectHelpers::ResolveActorClassPath(const FSoftObjectPath& Path)
{
	if (!Path.IsValid())
	{
		return Path;
	}

	const TMap<FString, FSoftClassPath>& Redirectors = UEMSPluginSettings::Get()->RuntimeClasses;
	if (Redirectors.IsEmpty())
	{
		return Path;
	}

	const FString PathString = Path.ToString();

	//Try exact match first
	if (const FSoftClassPath* RedirectedClass = Redirectors.Find(PathString))
	{
		return FSoftObjectPath(RedirectedClass->ToString());
	}

	//If no _C, try again with generated class suffix
	const FString ClassSuffix = TEXT("_C");
	if (!PathString.EndsWith(ClassSuffix))
	{
		const FString ClassPath = PathString + ClassSuffix;
		if (const FSoftClassPath* RedirectedClass = Redirectors.Find(ClassPath))
		{
			return FSoftObjectPath(RedirectedClass->ToString());
		}
	}

	return Path;
}

FSoftObjectPath FRedirectHelpers::ResolveLevelPath(const FSoftObjectPath& Path)
{
	if (!Path.IsValid())
	{
		return Path;
	}

	const TMap<FString, FString>& Redirectors = UEMSPluginSettings::Get()->LevelRedirects;
	if (Redirectors.IsEmpty())
	{
		return Path;
	}

	const FString PathString = Path.ToString();

	//Direct Lookup
	if (const FString* RedirectedPath = Redirectors.Find(PathString))
	{
		return FSoftObjectPath(*RedirectedPath);
	}

	//Package Lookup
	const FString PackageName = Path.GetLongPackageName();
	if (const FString* RedirectedPackage = Redirectors.Find(PackageName))
	{
		const FString SubPath = Path.GetSubPathString();

		/*
		Example scenario:
		Old: /Game/OldMap.OldMap:PersistentLevel.Actor
		Redirected Package: /Game/NewMap
		*/

		if (!SubPath.IsEmpty())
		{
			/*
			We need to construct: /Game/NewMap.NewMap:PersistentLevel.Actor
			Get the pure map name from the package (eg. "NewMap" from "/Game/NewMap")
			*/
			const FString NewMapName = FPaths::GetBaseFilename(*RedirectedPackage);

			/*
			Construct the full path...
			Note the structure: Package + Dot + MapName + Colon + SubPath
			The colon is still there, separating the Asset (Map) from the SubPath (Actor).
			*/
			const FString NewPath = FString::Printf(TEXT("%s.%s:%s"), **RedirectedPackage, *NewMapName, *SubPath);

			return FSoftObjectPath(NewPath);
		}

		/*
		If there is no subpath, just point to the map asset itself
		"/Game/NewMap.NewMap"
		*/
		const FString NewMapName = FPaths::GetBaseFilename(*RedirectedPackage);
		return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), **RedirectedPackage, *NewMapName));
	}

	return Path;
}
