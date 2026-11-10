//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSAsyncWait.h"
#include "EMSObject.h"
#include "EMSFunctionLibrary.h"
#include "UObject/Package.h"
#include "TimerManager.h"

UEMSAsyncWait::UEMSAsyncWait()
{
	bLoadTaskWasActive = false;
	bTimeoutTriggered = false;
	bCompleted = false;
	bCachedSaveFileExists = false;
	bCachedHasLevel = false;
	StartTime = 0.0;
	CheckCounter = 0;
}

UEMSAsyncWait* UEMSAsyncWait::AsyncWaitForOperation(UObject* WorldContextObject, ESaveGameMode Mode, TSoftObjectPtr<AActor> CheckActor)
{
	//Always return a valid task so the Blueprint node is guaranteed to fire its output pin. 
	UEMSAsyncWait* WaitTask = NewObject<UEMSAsyncWait>(GetTransientPackage());
	WaitTask->TargetActor = CheckActor;
	WaitTask->LoadMode = Mode;
	WaitTask->EMS = UEMSObject::Get(WorldContextObject);

	if (!WaitTask->EMS)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("AsyncWaitForOperation: EMS could not be resolved from the World Context. The node will complete immediately."));
	}

	return WaitTask;
}

static float GetWaitTime()
{
	const float WaitTime = FMath::Max(EMS::FallbackWaitTime, UEMSPluginSettings::Get()->AsyncWaitDelay);
	return WaitTime;
}

void UEMSAsyncWait::Activate()
{
	UWorld* World = EMS ? EMS->GetWorld() : nullptr;
	if (!World)
	{
		CompleteWaitTask();
		return;
	}

	bCachedSaveFileExists = EMS->DoesSaveSlotExist(EMS->GetCurrentSaveGameName());
	bCachedHasLevel = EMS->WasCurrentLevelSaved();

	//Arm the timeout BEFORE starting the task. Never register a timer on an object that is ready to destroy.
	const float WaitTime = GetWaitTime();
	StartTime = World->GetTimeSeconds();

	EMS->GetTimerManager().SetTimer(WaitTimerHandle, FTimerDelegate::CreateUObject(this, &UEMSAsyncWait::ForceProceed), WaitTime, false);
	StartWaitTask();
}

void UEMSAsyncWait::StartWaitTask()
{
	UWorld* World = EMS ? EMS->GetWorld() : nullptr;
	if (!World || bTimeoutTriggered)
	{
		CompleteWaitTask();
		return;
	}

	//Framerate independent wait
	double ElapsedTime = World->GetTimeSeconds() - StartTime;
	const float WaitTime = GetWaitTime();

	//Single Actor check
	if (!TargetActor.ToSoftObjectPath().IsNull())
	{
		AActor* ResolvedActor = TargetActor.Get();
		if (ResolvedActor && UEMSFunctionLibrary::IsLoadedState(ResolvedActor))
		{
			CompleteWaitTask();
			return;
		}

		//If the actor never reaches its loaded state, give up. Instead of waiting forever. 
		if (ElapsedTime >= WaitTime)
		{
			bTimeoutTriggered = true;
			CompleteWaitTask();
			return;
		}

		//If not loaded yet OR not finished processing, check again next tick
		EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncWait::StartWaitTask);
		return;
	}

	//Check for World Partition Init, irrelevant for player only mode
	bool bWorldPartitionInitalizing = false;
	if (!UEMSObject::SkipInitialWorldPartitionLoad() && LoadMode != ESaveGameMode::MODE_Player)
	{
		bWorldPartitionInitalizing = EMS->AutoSaveLoadWorldPartition() && !EMS->WorldPartitionLoadComplete();
	}

	//Does not catch Tasks started before the node, but also waits during pause
	const bool bIsTaskActive = FAsyncSaveHelpers::IsAsyncLoadTaskActive(LoadMode, false) || World->IsPaused();

	//Wait as long as the current async operation is completed.
	if (bIsTaskActive || bWorldPartitionInitalizing)
	{
		EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncWait::StartWaitTask);
		bLoadTaskWasActive = true;
	}
	else
	{
		//Make sure WP is finished
		const bool bWorldPartitionDone =
			LoadMode == ESaveGameMode::MODE_Player
			|| !EMS->AutoSaveLoadWorldPartition()
			|| EMS->WorldPartitionLoadComplete();

		if (!bCachedSaveFileExists || !bCachedHasLevel || bWorldPartitionDone)
		{
			//Wait a few ticks due to the latent nature of Save and Load operations
			if (++CheckCounter > EMS::MinAsyncWaitFrames)
			{
				bLoadTaskWasActive = true;
			}
		}
		else if (ElapsedTime >= WaitTime)
		{
			//Backstop for the timing race where an async task was expected but never observed active. 
			bTimeoutTriggered = true;
			bLoadTaskWasActive = true;
		}

		if (bLoadTaskWasActive)
		{
			CompleteWaitTask();
		}
		else
		{
			EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncWait::StartWaitTask);
		}
	}
}

void UEMSAsyncWait::ForceProceed()
{
	bTimeoutTriggered = true;
	StartWaitTask();
}

void UEMSAsyncWait::CompleteWaitTask()
{
	if (bCompleted)
	{
		return;
	}

	bCompleted = true;

	if (EMS)
	{
		EMS->GetTimerManager().ClearAllTimersForObject(this);
	}

	//bTimeoutTriggered is set by ForceProceed() and by the frame based backstops
	if (bTimeoutTriggered)
	{
		UE_LOG(LogEasyMultiSave, Warning, TEXT("AsyncWaitForOperation completed via timeout/backstop rather than a clean detection."));
	}

	OnCompleted.Broadcast();
	SetReadyToDestroy();
}