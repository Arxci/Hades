//Easy Multi Save - Copyright (C) 2026 by Michael Hegemann.  


#include "EMSMisc.h"
#include "EMSData.h"
#include "EMSPluginSettings.h"
#include "EMSAsyncLoadGame.h"
#include "EMSAsyncSaveGame.h"
#include "EMSAsyncStream.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

/**
FSettingHelpers
**/

bool FSettingHelpers::IsNoMultiLevelSave()
{
	return UEMSPluginSettings::Get()->MultiLevelSaving == EMultiLevelSaveMethod::ML_Disabled;
}

bool FSettingHelpers::IsNormalMultiLevelSave()
{
	return UEMSPluginSettings::Get()->MultiLevelSaving == EMultiLevelSaveMethod::ML_Normal;
}

bool FSettingHelpers::IsStreamMultiLevelSave()
{
	return UEMSPluginSettings::Get()->MultiLevelSaving == EMultiLevelSaveMethod::ML_Stream;
}

bool FSettingHelpers::IsFullMultiLevelSave()
{
	return UEMSPluginSettings::Get()->MultiLevelSaving == EMultiLevelSaveMethod::ML_Full;
}

bool FSettingHelpers::IsStackBasedMultiLevelSave()
{
	return IsFullMultiLevelSave() || IsNormalMultiLevelSave();
}

bool FSettingHelpers::IsContainingStreamMultiLevelSave()
{
	return IsFullMultiLevelSave() || IsStreamMultiLevelSave();
}

bool FSettingHelpers::IsMultiLevelSaveCompatible(const EMultiLevelSaveMethod InMode)
{
	const EMultiLevelSaveMethod CurrentSettings = UEMSPluginSettings::Get()->MultiLevelSaving;

	if (CurrentSettings == InMode)
	{
		return true;
	}

	switch (InMode)
	{

	case EMultiLevelSaveMethod::ML_Disabled:
	case EMultiLevelSaveMethod::ML_Stream:
		return IsNoMultiLevelSave() || IsStreamMultiLevelSave();

	case EMultiLevelSaveMethod::ML_Full:
	case EMultiLevelSaveMethod::ML_Normal:
		return IsStackBasedMultiLevelSave();

	default:
		break;
	}

	return false;
}

bool FSettingHelpers::IsDynamicLevelStreaming()
{
	return IsContainingStreamMultiLevelSave() && UEMSPluginSettings::Get()->bDynamicLevelStreaming;
}

bool FSettingHelpers::IsConsoleFileSystem()
{
	return UEMSPluginSettings::Get()->FileSaveMethod != EFileSaveMethod::FM_Desktop;
}

bool FSettingHelpers::IsMultiThreadSaving()
{
	return UEMSPluginSettings::Get()->bMultiThreadSaving && FPlatformProcess::SupportsMultithreading();
}

bool FSettingHelpers::IsMultiThreadLoading()
{
	return UEMSPluginSettings::Get()->LoadMethod == ELoadMethod::LM_Thread && FPlatformProcess::SupportsMultithreading();
}

bool FSettingHelpers::IsDeferredLoading()
{
	return UEMSPluginSettings::Get()->LoadMethod == ELoadMethod::LM_Deferred;
}

uint32 FSettingHelpers::GetLoadBatchSize()
{
	return FMath::Max(1, UEMSPluginSettings::Get()->DeferredLoadStackSize);
}

bool FSettingHelpers::IsSaveReportEnabled()
{
	return UEMSPluginSettings::Get()->SaveReport != ESaveReportSetting::Disabled;
}

/**
Async Node Helpers
**/

static bool ModesOverlap(const ESaveGameMode A, const ESaveGameMode B)
{
	return A == B || A == ESaveGameMode::MODE_All || B == ESaveGameMode::MODE_All;
}

template<class T>
bool FAsyncSaveHelpers::CheckLoadIterator(const T& It, const ESaveGameMode Mode, const bool bLog, const FString& DebugString)
{
	//Save and load may not run concurrently with overlapping modes, so always check both directions.
	if (It && It->IsActive() && ModesOverlap(It->Mode, Mode))
	{
		if (bLog)
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("%s is active while trying to perform another task."), *DebugString);
		}

		return true;
	}

	return false;
}

bool FAsyncSaveHelpers::IsAsyncLoadTaskActive(const ESaveGameMode Mode, const bool bLog)
{
	for (TObjectIterator<UEMSAsyncLoadGame> It; It; ++It)
	{
		if (CheckLoadIterator(It, Mode, bLog, "Load Game Actors"))
		{
			return true;
		}
	}

	for (TObjectIterator<UEMSAsyncStream> It; It; ++It)
	{
		if (CheckLoadIterator(It, Mode, bLog, "Load Stream Level Actors"))
		{
			return true;
		}
	}

	return false;
}

bool FAsyncSaveHelpers::IsAsyncTaskActive(const ESaveGameMode Mode, const bool bLog)
{
	//Ignores world pause for synchronous callers like cache clears, cloud IO and file checks

	if (IsAsyncLoadTaskActive(Mode, bLog))
	{
		return true;
	}

	for (TObjectIterator<UEMSAsyncSaveGame> It; It; ++It)
	{
		if (CheckLoadIterator(It, Mode, bLog, "Save Game Actors"))
		{
			return true;
		}
	}

	return false;
}

bool FAsyncSaveHelpers::IsAsyncTaskActiveWithPause(const UWorld* InWorld, const ESaveGameMode Mode, const bool bLog)
{
	if (!InWorld)
	{
		return false;
	}

	//This will prevent the functions from being executed at all during pause.
	if (InWorld->IsPaused())
	{
		if (bLog)
		{
			UE_LOG(LogEasyMultiSave, Warning, TEXT("Async operation called during pause. Operation was canceled."));
		}

		return true;
	}

	return IsAsyncTaskActive(Mode, bLog);
}

bool FAsyncSaveHelpers::IsStreamAutoLoadActive(const ULevel* InLevel)
{
	for (TObjectIterator<UEMSAsyncStream> It; It; ++It)
	{
		if (It && It->IsActive() && It->StreamingLevel == InLevel)
		{
			return true;
		}
	}

	return false;
}

void FAsyncSaveHelpers::DestroyStreamAutoLoadTask(const ULevel* InLevel)
{
	for (TObjectIterator<UEMSAsyncStream> It; It; ++It)
	{
		if (It && It->IsActive())
		{
			if (InLevel == nullptr || It->StreamingLevel == InLevel)
			{
				It->ForceDestroy();
			}
		}
	}
}

void FAsyncSaveHelpers::DestroyAsyncLoadTask()
{
	for (TObjectIterator<UEMSAsyncLoadGame> It; It; ++It)
	{
		if (It && It->IsActive())
		{
			It->ForceDestroy();
		}
	}
}

void FAsyncSaveHelpers::DestroyAsyncSaveTask()
{
	for (TObjectIterator<UEMSAsyncSaveGame> It; It; ++It)
	{
		if (It && It->IsActive())
		{
			It->ForceDestroy();
		}
	}
}

bool FAsyncSaveHelpers::IsWorldTearingDown(const UWorld* World)
{
	//Check if the world is being cleaned up
	if (!IsValid(World))
	{
		return true;
	}

	//Primary check - the most reliable flag
	if (World->bIsTearingDown)
	{
		return true;
	}

	//For PIE specifically, check if the world is valid for PIE
#if WITH_EDITOR
	if (World->IsPlayInEditor() && !World->IsValidLowLevel())
	{
		return true;
	}
#endif

	return false;
}

bool FAsyncSaveHelpers::ShouldCancelSaveTask(const UObject* InObject)
{
	if (!IsValid(InObject))
	{
		return true;
	}

	//Engine shutting down
	if (IsEngineExitRequested())
	{
		return true;
	}

	//World state check
	if (const UWorld* World = InObject->GetWorld())
	{
		if (IsWorldTearingDown(World))
		{
			return true;
		}
	}
	//No World
	else
	{
		return true;
	}

	return false;
}

ESaveGameMode FAsyncSaveHelpers::GetMode(const int32 Data)
{
	if (EMSFLAG::IsPlayer(Data))
	{
		if (EMSFLAG::IsLevel(Data))
		{
			return ESaveGameMode::MODE_All;
		}
		else
		{
			return ESaveGameMode::MODE_Player;
		}
	}

	return ESaveGameMode::MODE_Level;
}