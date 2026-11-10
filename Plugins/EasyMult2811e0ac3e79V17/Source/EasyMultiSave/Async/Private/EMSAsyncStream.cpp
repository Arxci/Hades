//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSAsyncStream.h"
#include "EMSObject.h"
#include "EMSActors.h"
#include "EMSData.h"
#include "EMSActorLoader.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "TimerManager.h"
#include "Async/Async.h"

/**
Init
**/

UEMSAsyncStream::UEMSAsyncStream()
{
	Mode = ESaveGameMode::MODE_All;
	bIsActive = false;
	bCompleted = false;
}

bool UEMSAsyncStream::InitStreamingLoadTask(UEMSObject* EMSObject, const ULevel* InLevel)
{
	//Check to see if an instance with the same streaming level is active.
	if (FAsyncSaveHelpers::IsStreamAutoLoadActive(InLevel))
	{
		UE_LOG(LogEasyMultiSave, Log, TEXT("Skipped loading streaming level while same async task is already active"));
		return false;
	}

	UEMSAsyncStream* LoadTask = NewObject<UEMSAsyncStream>(GetTransientPackage());
	if (LoadTask)
	{
		LoadTask->StreamingLevel = InLevel;
		LoadTask->EMS = EMSObject;
		LoadTask->RegisterWithGameInstance(EMSObject);
		LoadTask->Activate();
		return true;
	}

	return false;
}

void UEMSAsyncStream::Activate()
{
	if (EMS)
	{
		//Set active before any callback can run, so overlapping EMS operations are correctly detected
		bIsActive = true;

		/*
		Level Blueprints are not part of the Actor loader and load even if the level has no saved Actors.
		Must run before SetupLevelActors, so the loaded tag keeps the script out of the Actor map.
		*/
		const bool bScriptLoaded = LoadLevelScript();

		//Setup data one frame before loading
		if (!SetupLevelActors())
		{
			//A script-only level has no Actor loader, but it still applied valid save data
			if (bScriptLoaded)
			{
				EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncStream::FinishLoadingLevelScript);
			}
			else
			{
				Deactivate();
			}

			return;
		}

		EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncStream::StartLoad);
	}
}

void UEMSAsyncStream::StartLoad()
{
	if (EMS)
	{
		//Spawn Loader
		const FLoaderInitData LoaderData = FLoaderInitData(true, PrunedActorData, StreamActorsMap);
		Loader = FEMSLevelLoader::Create(EMS, LoaderData);
		Loader->OnComplete.BindUObject(this, &UEMSAsyncStream::FinishLoadingStreamLevel);
		Loader->Start();
	}
}

bool UEMSAsyncStream::LoadLevelScript()
{
	/*
	Streaming levels become visible after the main load chain already ran LoadLevelScripts,
	so their Level Blueprint is not in the ActorMap and has to be applied here instead.
	*/

	if (!EMS || !StreamingLevel)
	{
		return false;
	}

	AActor* ScriptActor = StreamingLevel->GetLevelScriptActor();
	if (!EMS->IsValidForLoading(ScriptActor) || FActorHelpers::IsSkipSave(ScriptActor))
	{
		return false;
	}

	//Same locked source as the streamed Actor data. SavedScripts is written by the save thread.
	const FName ScriptName = UEMSObject::LevelScriptSaveName(ScriptActor);

	FLevelScriptSaveData ScriptData;
	if (!EMS->GetStreamingLevelScriptData(ScriptName, ScriptData))
	{
		return false;
	}

	//Level Scripts have no components, so the struct pass only needs the Actor itself.
	FStructHelpers::SerializeStruct(ScriptActor);

	EMS->LoadActorFromBinary(ScriptActor, ScriptData.SaveData);
	UE_LOG(LogEasyMultiSave, Log, TEXT("%s Streaming Level Blueprint loaded"), *ScriptName.ToString());

	return true;
}

bool UEMSAsyncStream::SetupLevelActors()
{
	if (!EMS || !StreamingLevel || StreamingLevel->Actors.IsEmpty())
	{
		return false;
	}

	//Single locked snapshot
	const FMultiLevelStreamingData StreamSnapshot = EMS->GetMultiLevelStreamData();

	if (!StreamSnapshot.HasLevelActors())
	{
		return false;
	}

	const int32 EstimatedSize = StreamingLevel->Actors.Num();
	StreamActorsMap.Reserve(EstimatedSize);

	//This is somewhat like the Prepare Actors function
	for (AActor* Actor : StreamingLevel->Actors)
	{
		if (EMS->IsValidActor(Actor) 
			&& FActorHelpers::IsPlacedActor(Actor) 
			&& !FActorHelpers::IsLoaded(Actor) 
			&& !FActorHelpers::IsSkipSave(Actor))
		{
			const FString FullName = EMS->GetFullActorName(Actor);
			StreamActorsMap.Add(FActorByteKey(FullName), Actor);
		}
	}

	if (StreamActorsMap.IsEmpty())
	{
		return false;
	}

	//Prune data
	PrunedActorData = StreamSnapshot.ActorArray;
	FActorHelpers::PruneSavedActors(StreamActorsMap, PrunedActorData);

	//Count what will actually be applied, not the candidate map size
	TotalActors = PrunedActorData.Num();
	EMS->AllocateRealLoadedActors(TotalActors);

	return true;
}

/**
Finish
**/

void UEMSAsyncStream::FinishLoadingStreamLevel()
{
	UE_LOG(LogEasyMultiSave, Log, TEXT("Loaded %d streaming Actors"), TotalActors);
	EndTask(true);
}

void UEMSAsyncStream::FinishLoadingLevelScript()
{
	//No Actor loader ran, the Level Blueprint was the only saved data in this level
	UE_LOG(LogEasyMultiSave, Log, TEXT("Loaded streaming Level Blueprint data without Actors"));
	EndTask(true);
}

void UEMSAsyncStream::Deactivate()
{
	EndTask(false);
}

void UEMSAsyncStream::ForceDestroy()
{
	//The task ends here, but the Loader has its own timers and thread work that must be canceled too
	if (Loader)
	{
		Loader->Destroy();
	}

	EndTask(false);
}

void UEMSAsyncStream::EndTask(const bool bBroadcastFinish)
{
	//Reached by both loader completion and ForceDestroy
	if (bCompleted)
	{
		return;
	}

	bCompleted = true;
	bIsActive = false;

	if (EMS)
	{
		EMS->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (bBroadcastFinish && EMS)
	{
		EMS->BroadcastOnPartitionLoaded();
	}

	SetReadyToDestroy();
}