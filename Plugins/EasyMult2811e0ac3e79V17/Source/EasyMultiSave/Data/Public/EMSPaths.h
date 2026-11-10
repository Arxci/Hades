//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "CoreMinimal.h"

struct FSaveSlotInfo;
struct FImage;
class FArchive;
class UTextureRenderTarget2D;
class UWorld;

class EASYMULTISAVE_API FSavePaths
{

public:

	static TArray<FString> GetDefaultSaveFiles(const FString& SaveGameName);

	static FString GetBackupSavePath(const FString& FullSavePath);
	static FString GetBackupFileName(const FString& BaseName);
	static FString GetBaseSavePath(const FString& FullSavePath);

	static TArray<FString> GetSortedSaveSlots(TArray<FSaveSlotInfo>& SaveSlots);

	static bool IsFileNameReserved(const FString& SaveGameName);

	/** Slot names that would collide with internal EMS paths. Expects an already validated name. */
	static bool IsSlotNameReserved(const FString& SlotName);

	static FString ValidateSaveName(const FString& SaveGameName);

	static FString GetThumbnailFormat();

	static void CheckForReadOnly(const FString& FullSavePath);

	static TArray<FString> GetConsoleSlotFiles(const TArray<FString>& SaveGameNames);

	static bool IsSafeToDelete(const FString& AbsoluteBaseDir, const FString& InputFolder);
	static bool DeleteSaveDirectory(const FString& BaseDir, const FString& InputName, const FString& Context);
};

class EASYMULTISAVE_API FSaveThumbnails
{

public:

	static bool CompressImage(const FImage& Image, FArchive& Ar);
};

class EASYMULTISAVE_API FEditorPaths
{

public:

#if WITH_EDITOR
	static void StripRefPIEPrefix(FString& PathString);
#endif

	static FString StripLevelPIEPrefix(const UWorld* InWorld, const FString& LevelName);
};

class EASYMULTISAVE_API FRedirectHelpers
{

private:

	template<typename MapType>
	static FString ResolveRedirect(const FString& Input, const MapType& RedirectMap);

public:

	static FString ResolveWorldLevel(const FString& LevelPath);
	static FString ResolveActorLevel(const FString& ShortLevelName);

	static FSoftObjectPath ResolveActorClassPath(const FSoftObjectPath& Path);
	static FSoftObjectPath ResolveLevelPath(const FSoftObjectPath& Path);
};

