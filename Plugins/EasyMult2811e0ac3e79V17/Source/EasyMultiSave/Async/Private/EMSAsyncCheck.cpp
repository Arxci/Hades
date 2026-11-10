//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  

#include "EMSAsyncCheck.h"
#include "EMSObject.h"
#include "UObject/Package.h"
#include "TimerManager.h"

UEMSAsyncCheck::UEMSAsyncCheck()
{
	Type = ESaveFileCheckType::CheckForGame;
	CheckResult = EIntegrityCheckResult::Success;
}

UEMSAsyncCheck* UEMSAsyncCheck::CheckSaveFiles(UObject* WorldContextObject, ESaveFileCheckType CheckType, FString CustomSaveName)
{
	if (UEMSObject* EMSObject = UEMSObject::Get(WorldContextObject))
	{
		//File checks are read-only and valid while paused, so only check the task objects
		if (!FAsyncSaveHelpers::IsAsyncTaskActive(ESaveGameMode::MODE_All, true))
		{
			UEMSAsyncCheck* CheckTask = NewObject<UEMSAsyncCheck>(GetTransientPackage());
			CheckTask->EMS = EMSObject;
			CheckTask->Type = CheckType;
			CheckTask->SaveFileName = CustomSaveName;
			return CheckTask;
		}
	}

	return nullptr;
}

void UEMSAsyncCheck::Activate()
{
	if (EMS)
	{
		EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::StartCheck);
	}
	else
	{
		CheckResult = EIntegrityCheckResult::Unknown;
		CompleteCheck();
	}
}

void UEMSAsyncCheck::StartCheck()
{
	switch (Type)
	{
	case ESaveFileCheckType::CheckForCustom:
		CheckCustom();
		break;

	case ESaveFileCheckType::CheckForCustomSlot:
		CheckCustomSlot();
		break;

	case ESaveFileCheckType::CheckForPlayerOnly:
		CheckResult = EMS->CheckSaveGameIntegrity(EMS->PlayerSaveFile());
		EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CompleteCheck);
		break;

	case ESaveFileCheckType::CheckForLevelOnly:
		CheckResult = EMS->CheckSaveGameIntegrity(EMS->ActorSaveFile());
		EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CompleteCheck);
		break;

	default:
		CheckResult = EMS->CheckSaveGameIntegrity(EMS->SlotInfoSaveFile());

		if (Type == ESaveFileCheckType::CheckForSlotOnly)
		{
			EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CompleteCheck);
		}
		else
		{
			//Continue chaining Slot > Player > Level 
			EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CheckPlayer);
		}
		break;
	}
}

void UEMSAsyncCheck::CheckPlayer()
{
	const EIntegrityCheckResult PlayerResult = EMS->CheckSaveGameIntegrity(EMS->PlayerSaveFile());

	//Only update CheckResult if it's better than the file result 
	if (CheckResult == EIntegrityCheckResult::Success)
	{
		CheckResult = PlayerResult;
	}

	EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CheckLevel);
}

void UEMSAsyncCheck::CheckLevel()
{
	const EIntegrityCheckResult LevelResult = EMS->CheckSaveGameIntegrity(EMS->ActorSaveFile());

	if (CheckResult == EIntegrityCheckResult::Success)
	{
		CheckResult = LevelResult;
	}

	EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CompleteCheck);
}

void UEMSAsyncCheck::CheckCustom()
{
	CheckResult = EMS->CheckSaveGameIntegrity(EMS->CustomSaveFile(SaveFileName, FString()));
	EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CompleteCheck);
}

void UEMSAsyncCheck::CheckCustomSlot()
{
	CheckResult = EMS->CheckSaveGameIntegrity(EMS->CustomSaveFile(SaveFileName, EMS->GetCurrentSaveGameName()));
	EMS->GetTimerManager().SetTimerForNextTick(this, &UEMSAsyncCheck::CompleteCheck);
}

void UEMSAsyncCheck::CompleteCheck()
{
	if (CheckResult == EIntegrityCheckResult::Success)
	{
		OnCompleted.Broadcast();
	}
	else if (CheckResult == EIntegrityCheckResult::VersionMismatch)
	{
		OnVersionMismatch.Broadcast();
	}
	else
	{
		OnFailed.Broadcast();
	}

	SetReadyToDestroy();
}
