//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSTypes.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h" 
#include "Engine/EngineTypes.h"

class AActor;
struct FActorSaveData;

/*
Advanced Save Report System

This acts as a human-readable text mirror of the binary save game data payload.
Extracted data is routed through a custom layout engine that parses and prettifies nested
structs, transforms, and arrays into a cleanly indented, hierarchical layout.
Output text files are bundled inside uniquely timestamped session folders within the project log directory,
providing an organized snapshot of the game disk-state to easily isolate save mismatches.

All property snapshots are held in a per-file in-memory cache (TMap keyed by object path).
CommitSaveReport() writes all dirty cache entries to disk and marks them clean.
The cache is NOT cleared, preserving snapshots for streaming levels.
Use ClearReportCache() or DeleteCurrentSessionFolder() to manually clear.
*/

struct FSaveReportData
{

public:

	FString PropertyName;
	FString PropertyType;
	FString CurrentValue;
	bool bIsSaveGameFlagged = false;
	bool bIsStructType = false;
};

struct FSaveReportContextBase
{

public:

	ESaveObjectType DataType = ESaveObjectType::SaveGameOnly;

	FString UserName = FString();
	FString SlotName = TEXT("UnknownSlot");

	FString CollectionName = FString();
	bool bCollectionInSlot = false;

	bool bIsLoading = false;

	FString PluginVersion;
	FString GameVersion;

public:

	inline bool IsCollection() const
	{
		return !CollectionName.IsEmpty();
	}

	inline bool IsCollectionInSlot() const
	{
		return bCollectionInSlot && IsCollection();
	}
};

struct FSaveReportContext : FSaveReportContextBase
{

public:

	//Raw pointer, make sure not to use without validation
	const UObject* TargetObject = nullptr;

public:
	
	FSaveReportContext() = default;
	explicit FSaveReportContext(const UObject* InTarget) : TargetObject(InTarget) {}

	//Any Object (Actor, Component, Custom Save)
	static FSaveReportContext ForObject(const UObject* InTarget, ESaveObjectType InDataType, bool bIsLoading)
	{
		FSaveReportContext Ctx(InTarget);
		Ctx.DataType = InDataType;
		Ctx.bIsLoading = bIsLoading;
		return Ctx;
	}

	//Object Collection 
	static FSaveReportContext ForCollection(const UObject* InTarget, const FString& InCollectionName, bool bInSlot, bool bIsLoading)
	{
		FSaveReportContext Ctx(InTarget);
		Ctx.CollectionName = InCollectionName;
		Ctx.bCollectionInSlot = bInSlot;
		Ctx.bIsLoading = bIsLoading;
		Ctx.DataType = ESaveObjectType::SaveGameOnly;
		return Ctx;
	}
};

#if EMS_BUILD_DEVELOPER

struct FReportFileCache
{
	//Context captured on first write
	FSaveReportContextBase BaseContext;

	//Object path marke, full snapshot block string
	TMap<FString, FString> BlocksByPath;

	//Ordered list of path markers reflecting first-insertion order
	TArray<FString> InsertionOrder;

	//True when the cache contains data not yet committed to disk
	bool bDirty = false;

	//Cached session header content
	FString OperationName;
	FString CachedMapName;
	ENetMode CachedNetMode = NM_Standalone;
	bool bHasValidWorld = false;
};


class EASYMULTISAVE_API FSaveReport
{

public:

	static void WriteSaveReport(const FSaveReportContext& Context);
	static void CommitSaveReport();
	static void ClearReportCache(const bool bFullClear = true);
	static void DeleteCurrentSessionFolder();
};

#endif

class EASYMULTISAVE_API FSaveLogDebug
{

public:

	static void DebugLogDataSize(const UObject* Object, const TArray<uint8>& InData);
	static void LogActorOperation(const FActorSaveData& ActorData, const bool bIsLoading);
	static void LogPlayerOperation(const AActor* Actor, const TCHAR* PlayerType, const bool bIsLoading, const bool bIsValid);
	static void LogSavedDestroyedActors(const TArray<FActorSaveData>& DestroyedActors);
};
