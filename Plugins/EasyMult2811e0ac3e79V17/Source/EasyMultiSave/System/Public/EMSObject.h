//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#pragma once

#include "EMSObjectBase.h"
#include "Engine/LevelStreamingDynamic.h"
#include "UObject/ObjectKey.h"
#include "EMSObject.generated.h"

class UEMSCustomSaveGame;
class UEMSInfoSaveGame;
class UEMSAsyncSaveGame;
class UEMSAsyncLoadGame;
class FBufferArchive;
class FMemoryReader;
class APawn;
class APlayerController;
class AController;
class AActor;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEmsLoadPlayerComplete, const APlayerController*, LoadedPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEmsLoadLevelComplete, const TArray<TSoftObjectPtr<AActor>>&, LoadedActors);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEmsSaveActorsComplete, ESaveGameMode, Mode, bool, bSuccess);

UCLASS(BlueprintType, meta = (DisplayName = "Easy Multi Save", Keywords = "Save, EMS, EasyMultiSave, EasySave"))
class EASYMULTISAVE_API UEMSObject : public UEMSObjectBase
{
	GENERATED_BODY()

protected:
	UEMSObject();

/** Variables */

public:

	UPROPERTY(BlueprintAssignable, Category = "Easy Multi Save | Delegates", meta = (DisplayName = "On Player Actors Loaded"))
	FEmsLoadPlayerComplete OnPlayerLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Easy Multi Save | Delegates", meta = (DisplayName = "On Persistent Level Loaded"))
	FEmsLoadLevelComplete OnLevelLoaded;

	//Previously only covered world partition, now covering automatic streaming load as well
	UPROPERTY(BlueprintAssignable, Category = "Easy Multi Save | Delegates", meta = (DisplayName = "On Streaming Level Loaded"))
	FEmsLoadLevelComplete OnPartitionLoaded;

	//Fires when a Save Game Actors node completes, successful or not. Automatic streaming saves are excluded.
	UPROPERTY(BlueprintAssignable, Category = "Easy Multi Save | Delegates", meta = (DisplayName = "On Game Actors Saved"))
	FEmsSaveActorsComplete OnActorsSaved;

private:

	mutable FCriticalSection SaveActorsScope;
	mutable FCriticalSection RawObjectScope;

	FString CachedPersistentLevelName;

	std::atomic<bool> bLoadFromMemory = false;

	uint8 bInitWorldPartitionLoadComplete = false;
	uint8 bTryInitWorldPartitionLoad = false;

	uint8 bSaveStreaming : 1;

	uint8 bCachedIsWorldPartition : 1;
	uint8 bCachedHasStreamingLevels : 1;
	
	float WorldPartitionInitTimer;
	float StreamingSaveRequestTime;

	FDelegateHandle ActorDestroyedHandle;

/** Data Archive Cache Variables, Zero initialized(Transient) */

private:

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> ActorList;

	UPROPERTY(Transient)
	TMap<FActorByteKey, TWeakObjectPtr<AActor>> ActorMap;

private:

	UPROPERTY(Transient)
	TArray<FLevelArchive> LevelArchiveList;	

	UPROPERTY(Transient)
	FMultiLevelStreamingData MultiLevelStreamData;
	
	UPROPERTY(Transient)
	TArray<FActorSaveData> SavedActors;

	UPROPERTY(Transient)
	TArray<FActorSaveData> SavedActorsPruned;

	UPROPERTY(Transient)
	TArray<FLevelScriptSaveData> SavedScripts;

	UPROPERTY(Transient)
	FGameObjectSaveData SavedGameMode;

	UPROPERTY(Transient)
	FGameObjectSaveData SavedGameState;

private:

	UPROPERTY(Transient)
	TSet<FActorSaveData> StreamingActors;

	UPROPERTY(Transient)
	TSet<FActorSaveData> DestroyedActors;

	UPROPERTY(Transient)
	TArray<TSoftObjectPtr<AActor>> RealLoadedActors;

	UPROPERTY(Transient)
	TMap<FActorByteKey, FActorSaveData> RawObjectLevelActors;

	TMap<FObjectKey, FGameObjectSaveData> RawObjectData;

private:

	UPROPERTY(Transient)
	FPlayerStackArchive PlayerStackData;

	UPROPERTY(Transient)
	FPlayerArchive SavedPlayer;

/** Blueprint Library function accessors */
	
public:

	UObject* LoadRawObject(const AActor* Actor, const FRawObjectSaveData& Data);
	bool SaveRawObject(const AActor* Actor, const FRawObjectSaveData& Data);

	bool SavePlayerActorsCustom(AController* Controller, const FString& FileName);
	bool LoadPlayerActorsCustom(AController* Controller, const FString& FileName);
	bool DeleteCustomPlayerFile(const FString& FileName);

/** Other public Functions  */

public:

	static UEMSObject* Get(const UObject* WorldContextObject);

	void PrepareLoadAndSaveActors(const uint32 Data, const EAsyncCheckType FunctionType, const EPrepareType PrepareType);

	bool SavePlayerActors(const APlayerController* Controller, const FString& FileName, const bool bCustomFile = false);
	void LoadPlayerActors(APlayerController* Controller);

	bool SaveLevelActors(const bool bMemoryOnly, const FLevelSnapshot& Snapshot);
	void LoadLevelActors(UEMSAsyncLoadGame* LoadTask);
	void LoadGameMode();
	void LoadLevelScripts();
	bool GetStreamingLevelScriptData(const FName ScriptName, FLevelScriptSaveData& OutData) const;

	void FinishLoadingLevel(const bool bHasLoadedFile);

	bool TryLoadPlayerFile();
	bool TryLoadLevelFile();

	APlayerController* GetPlayerController() const;
	APawn* GetPlayerPawn(const APlayerController* PC) const;

	FTimerManager& GetTimerManager() const;

	bool HasValidGameWorld() const;
	bool HasValidPlayer() const;

/** Internal Functions  */

public:

	void SaveActorToBinary(const AActor* Actor, FGameObjectSaveData& OutData) const;
	void LoadActorFromBinary(AActor* Actor, const FGameObjectSaveData& InData, const bool bDeferredSpawn = false) const;

	void SpawnLevelActor(const FActorSaveData& ActorArray);
	void ProcessLevelActor(AActor* Actor, const FActorSaveData& ActorArray, const bool bSetupStructs, const bool bDeferredSpawn = false);

	void PruneLevelActors();

protected:

	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;

private:

	void CreateLevelActor(UClass* SpawnClass, const FActorSaveData& ActorArray, const FActorSpawnParameters& SpawnParams);
	void FailSpawnLevelActor(const FActorSaveData& ActorArray) const;

	bool UnpackActorArchive(FMemoryReader& FromBinary, const EDataLoadType LoadType) override;
	bool UnpackLevelArchive(FMemoryReader& FromBinary);
	bool UnpackPlayerArchive(FMemoryReader& FromBinary, const bool bCustomFile);
	bool UnpackLevel(const FLevelArchive& LevelArchive);
	void UnpackPlayer(const FPlayerArchive& PlayerArchive);

	void PrepareLevelActors(const FActorInitContext& Init);
	void PreparePlayerActors(const FActorInitContext& Init);
	void AddActorToList(const FActorInitContext& Init);
	void InitGameActor(const FActorInitContext& Init);
	
	FGameObjectSaveData ParseGameModeObjectForSaving(const AActor* Actor) const;
	FLevelScriptSaveData ParseLevelScriptForSaving(const AActor* Actor) const;
	FActorSaveData ParseLevelActorForSaving(const AActor* Actor, const EActorType Type) const;

	static void ProcessSaveEvent(const TWeakObjectPtr<const UObject> WeakRef, ESaveEventType SaveType);
	void ExecuteSaveEvent(const UObject* InObject, const ESaveEventType SaveType) const;
	void ExecuteActorPreSave(const AActor* Actor) const;
	void ExecuteActorSaved(const AActor* Actor) const;
	void ExecuteActorPreLoad(AActor* Actor) const;
	void ExecuteActorLoaded(AActor* Actor) const;
	void ExecuteComponentPreSave(const UActorComponent* Comp) const;
	void ExecuteComponentSaved(const UActorComponent* Comp) const;

	void SerializeActorStructProperties(const AActor* Actor) const;

	TArray<UActorComponent*> GetSaveComponents(const AActor* Actor) const;
	void SaveActorComponents(const AActor* Actor, TArray<FComponentSaveData>& OutComponents) const;
	void LoadActorComponents(const AActor* Actor, const TArray<FComponentSaveData>& InComponents) const;

	UObject* SerializeFromRawObject(const AActor* Actor, const FRawObjectSaveData& Data, const TArray<FComponentSaveData>& InputArray);
	void AppendRawObjectData(const AActor* Actor, TArray<FComponentSaveData>& OutComponents) const;
	void UpdateRawObjectData(const AActor* Actor, const FComponentSaveData& InputData);

	FLevelStackArchive AddMultiLevelStackData(const FLevelArchive& LevelArchive, const FLevelArchive& PersistentArchive, const FGameObjectSaveData& InGameMode, const FGameObjectSaveData& InGameState);
	FLevelArchive AddMultiLevelStreamData(const FLevelArchive& LevelArchive);
	void UpdateMultiLevelStreamData(const FLevelArchive& LevelArchive);

/** Actor Delegates  */

private:

	void OnActorDestroyed(AActor* DestroyedActor);
	void RemoveActorDelegates(UWorld* World);

/** World Delegates  */

	void OnPreWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	void OnLevelStreamingStateChanged(UWorld* InWorld, const ULevelStreaming* InStreamingLevel, ULevel* InLevelIfLoaded, ELevelStreamingState PreviousState, ELevelStreamingState NewState);
	void OnLevelBeginMakingInvisible(UWorld* InWorld, const ULevelStreaming* InStreamingLevel, ULevel* InLoadedLevel);
	void RemoveWorldStreamDelegates();

/** General Streaming Functions  */

	void HandleStreamingPersistence(const UWorld* InWorld, const ULevel* InLevel, const bool bIsLoading);
	bool ProcessSaveStreamingActors(const ULevel* InLoadedLevel);
	void StartStreamingSave(const ULevel* InLoadedLevel);
	void StartStreamingLoad(const ULevel* InLoadedLevel);
	void TryAccumulatedStreamingSave();

/** World Partition Functions  */

private:

	void WorldPartitionLoadLevel(const ULevel* InLoadedLevel);
	void WorldPartitionSaveLevel(const UWorld* InWorld, const ULevel* InLoadedLevel);

	void TryInitWorldPartition();
	void PollInitWorldPartition();

/** Delegate Functions  */

public:

	inline void BroadcastOnPartitionLoaded()
	{
		OnPartitionLoaded.Broadcast(RealLoadedActors);
		RealLoadedActors.Empty();
	}

	inline void BroadcastOnLevelLoaded()
	{
		OnLevelLoaded.Broadcast(RealLoadedActors);
		RealLoadedActors.Empty();
	}

	inline void BroadcastOnActorsSaved(const ESaveGameMode InMode, const bool bSuccess)
	{
		OnActorsSaved.Broadcast(InMode, bSuccess);
	}

	inline void AllocateRealLoadedActors(const int32 AllocNum)
	{
		RealLoadedActors.Reserve(AllocNum);
	}

	inline bool HasActuallyLoadedActors()
	{
		return !RealLoadedActors.IsEmpty();
	}

/** Clear Data Functions  */

public:

	void ClearMultiLevelSave();
	void ClearWorldPartition();
	void ClearUserData() override;

protected:

	void ClearLoadFromMemory() override;

private:

	inline void ClearPlayerPosition()
	{
		SavedPlayer.ClearPosition();
	}

	inline void ClearActorList()
	{
		ActorList.Empty();
		ActorMap.Empty();
		ClearRawObjectData();
	}

	inline void ClearRawObjectData()
	{
		FScopeLock Lock(&RawObjectScope);
		RawObjectData.Empty();
	}

	inline void ClearSavedLevelActors()
	{
		SavedActors.Empty();
		SavedScripts.Empty();
		SavedActorsPruned.Empty();
	}

	inline void ClearStreamingData()
	{
		MultiLevelStreamData = FMultiLevelStreamingData();
	}

	inline void ClearStreamingActors()
	{
		StreamingActors.Empty();
	}

	inline void ClearDestroyedActors()
	{
		DestroyedActors.Empty();
	}

	inline void ClearRawObjectLevelActors()
	{
		RawObjectLevelActors.Empty();
	}

/** Actor Helpers  */

public:

	FName GetLevelName();
	void AssignLevelName(const UWorld* InWorld);
	bool MatchesLevelName(const FName& InLevelName);

	bool IsDifferentGameWorld(const UWorld* InWorld) const;
	void RemoveActor(AActor* Actor);

	static FName LevelScriptSaveName(const AActor* Actor);
	static EActorType GetActorType(const AActor* Actor);
	static FString GetActorLevelName(const AActor* Actor);
	static FString GetFullActorName(const AActor* Actor);

	static bool IsLoaded(const AActor* Actor);
	static bool IsPlaced(const AActor* Actor);
	static bool IsSkipSave(const AActor* Actor);
	static bool IsSkipTransform(const AActor* Actor);
	static bool IsLevelScript(const EActorType Type);
	static bool IsLevelActor(const EActorType Type, const bool bIncludeScripts);
	static bool IsRuntimeActor(const EActorType Type);
	static bool HasSaveInterface(const AActor* Actor);
	static bool HasComponentSaveInterface(const UActorComponent* Comp);

	FLevelSnapshot CaptureActorSnapshot(const bool bStreamingActorsOnly);

/** Other Helper Functions  */

public:

	inline static bool AutoDestroyActors()
	{
		return UEMSPluginSettings::Get()->bAutoDestroyActors;
	}

	inline bool IsValidActor(const AActor* Actor) const
	{
		return IsValid(Actor) && HasSaveInterface(Actor);
	}

	inline bool IsValidForSaving(const AActor* Actor) const
	{
		return IsValidActor(Actor) && !IsSkipSave(Actor);
	}

	inline bool IsValidForLoading(const AActor* Actor) const
	{
		return IsValidActor(Actor) && !IsLoaded(Actor);
	}

	inline bool HasLevelData() const
	{
		return !SavedActors.IsEmpty()
			|| !SavedScripts.IsEmpty()
			|| SavedGameMode.HasData()
			|| SavedGameState.HasData()
			|| MultiLevelStreamData.HasData();
	}

/** Streaming Helpers  */

public:

	inline bool HasAnyLevelStreaming() const
	{
		return bCachedHasStreamingLevels || bCachedIsWorldPartition;
	}

	inline bool IsLevelStreaming() const
	{
		return FStreamHelpers::IsLevelStillStreaming(GetWorld());
	}

	inline bool CanProcessStreaming() const
	{
		return FStreamHelpers::CanProcessStreaming(GetWorld());
	}

	inline bool AutoSaveLoadStreamingLevels()
	{
		//The Multi-Level check mirrors AutoSaveLoadWorldPartition, required since the setting is enabled by default
		return bCachedHasStreamingLevels
			&& FSettingHelpers::IsContainingStreamMultiLevelSave()
			&& UEMSPluginSettings::Get()->LevelStreamSaving != EAutoSaveStreamMethod::Disabled;
	}

	inline FMultiLevelStreamingData GetMultiLevelStreamData() const
	{
		FScopeLock Lock(&SaveActorsScope);
		return MultiLevelStreamData;
	}

	inline void RestoreStreamingActors(const TArray<FActorSaveData>& InActors)
	{
		//Returns snapshot data to the pending set after a failed save as their are cleared on snapshot.
		for (const FActorSaveData& ActorData : InActors)
		{
			StreamingActors.Add(ActorData);
		}
	}

/** World Partition Helpers  */

public:

	inline bool AutoSaveLoadWorldPartition() const
	{
		return bCachedIsWorldPartition;
	}

	inline bool CanProcessWorldPartition() const
	{
		return FStreamHelpers::CanProcessWorldPartition(GetWorld());
	}

	inline bool IsInitialWorldPartitionLoading() const
	{
		return !bInitWorldPartitionLoadComplete && IsAsyncTaskActiveWithPause(ESaveGameMode::MODE_Level, false);
	}

	inline static bool SkipInitialWorldPartitionLoad()
	{
		return UEMSPluginSettings::Get()->WorldPartitionInit == EWorldPartitionInit::Skip;
	}

	inline bool AlwaysAutoLoadWorldPartition() const
	{
		return bInitWorldPartitionLoadComplete && bLoadFromMemory;
	}

	inline bool WorldPartitionLoadComplete() const
	{
		return bInitWorldPartitionLoadComplete && !IsLevelStreaming() && !IsInitialWorldPartitionLoading();
	}
};
