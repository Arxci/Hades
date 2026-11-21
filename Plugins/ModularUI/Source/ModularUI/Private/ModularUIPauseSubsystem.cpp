#include "ModularUIPauseSubsystem.h"

#include "Components/PostProcessComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/Engine.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/PropertyPortFlags.h"

#include "ModularUI.h"
#include "ModularUISettings.h"

#if !UE_BUILD_SHIPPING
namespace
{
	UModularUIPauseSubsystem* FindPauseSubsystem(UWorld* World)
	{
		UModularUIPauseSubsystem* Pause = World ? World->GetSubsystem<UModularUIPauseSubsystem>() : nullptr;
		if (!Pause)
		{
			UE_LOG(LogModularUI, Display, TEXT("[Modular UI] No pause subsystem in %s. Run the command while playing, such as in Play In Editor."), *GetNameSafe(World));
		}
		return Pause;
	}

	FAutoConsoleCommandWithWorldAndArgs ForceLookCommand(
		TEXT("ModularUI.Pause.ForceLook"),
		TEXT("Shows the paused post process without pausing, to check it reaches the screen. 1 shows it, 0 goes back to following pause requests, nothing toggles it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UModularUIPauseSubsystem* Pause = FindPauseSubsystem(World))
			{
				Pause->SetPausedLookForced(Args.IsEmpty() ? !Pause->IsPausedLookForced() : FCString::Atoi(*Args[0]) != 0);
				Pause->LogStatus();
			}
		}));

	FAutoConsoleCommandWithWorld StatusCommand(
		TEXT("ModularUI.Pause.Status"),
		TEXT("Logs who is asking for the pause, and the paused post process's state."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UModularUIPauseSubsystem* Pause = FindPauseSubsystem(World))
			{
				Pause->LogStatus();
			}
		}));
}
#endif // !UE_BUILD_SHIPPING

UModularUIPauseSubsystem* UModularUIPauseSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UModularUIPauseSubsystem>() : nullptr;
}

bool UModularUIPauseSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UModularUIPauseSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(FadeHandle);
	FadeHandle = FTSTicker::FDelegateHandle();

	// The volume belongs to the world, which is going away with this.
	PausedLookVolume.Reset();

	Super::Deinitialize();
}

void UModularUIPauseSubsystem::AddPauseRequest(UObject* Requester)
{
	if (Requester)
	{
		PauseRequests.AddUnique(Requester);
		UpdatePause();
	}
}

void UModularUIPauseSubsystem::RemovePauseRequest(UObject* Requester)
{
	if (PauseRequests.Remove(Requester) > 0)
	{
		UpdatePause();
	}
}

void UModularUIPauseSubsystem::UpdatePause()
{
	PauseRequests.RemoveAll([](const TWeakObjectPtr<const UObject>& Requester) { return !Requester.IsValid(); });

	const bool bWantsPause = !PauseRequests.IsEmpty();
	if (bWantsPause == bPauseRequested)
	{
		return;
	}

	bPauseRequested = bWantsPause;
	UGameplayStatics::SetGamePaused(this, bWantsPause);
	StartFade();
	OnPauseChanged.Broadcast(bWantsPause);

	const TCHAR* PauseText = bWantsPause ? TEXT("paused") : TEXT("resumed");
	if (const UPostProcessComponent* PostProcess = PausedPostProcess.Get())
	{
		UE_LOG(LogModularUI, Log, TEXT("[Modular UI] World %s. The paused post process %s fades to a Blend Weight of %.2f."),
			PauseText, *GetPathNameSafe(PostProcess), bWantsPause ? PausedBlendWeight : 0.0f);
	}
	else if (GetDefault<UModularUISettings>()->HasPausedLook())
	{
		UE_LOG(LogModularUI, Log, TEXT("[Modular UI] World %s. Project Settings' Paused Look fades %s."), PauseText, bWantsPause ? TEXT("in") : TEXT("out"));
	}
	else
	{
		UE_LOG(LogModularUI, Log, TEXT("[Modular UI] World %s. No look fades in: set one in Project Settings > Plugins > Modular UI > Paused Look."), PauseText);
	}
}

void UModularUIPauseSubsystem::SetPausedPostProcess(UPostProcessComponent* PostProcess, float FadeInTime, float FadeOutTime)
{
	FadeInSeconds = FMath::Max(0.0f, FadeInTime);
	FadeOutSeconds = FMath::Max(0.0f, FadeOutTime);

	UPostProcessComponent* Previous = PausedPostProcess.Get();
	if (PostProcess != Previous)
	{
		if (Previous)
		{
			Previous->BlendWeight = PausedBlendWeight;
			Previous->bEnabled = false;
		}

		// The fade drives Blend Weight from here, up to whatever the component was given. It shows the look instead of Paused Look's volume.
		PausedPostProcess = PostProcess;
		FadeAlpha = 0.0f;
		if (APostProcessVolume* Volume = PausedLookVolume.Get())
		{
			Volume->BlendWeight = 0.0f;
			Volume->bEnabled = false;
		}
		if (PostProcess)
		{
			PausedBlendWeight = PostProcess->BlendWeight;
			PostProcess->BlendWeight = 0.0f;
			PostProcess->bEnabled = false;

			UE_LOG(LogModularUI, Log, TEXT("[Modular UI] Paused post process: %s, fading in to a Blend Weight of %.2f over %.2fs. Its Enabled and Blend Weight are driven by the pause from now on."),
				*GetPathNameSafe(PostProcess), PausedBlendWeight, FadeInSeconds);
			WarnIfNothingShows(*PostProcess);
		}
	}
	StartFade();
}

UPostProcessComponent* UModularUIPauseSubsystem::GetPausedPostProcess() const
{
	return PausedPostProcess.Get();
}

void UModularUIPauseSubsystem::WarnIfNothingShows(const UPostProcessComponent& PostProcess) const
{
	// Unreal skips post process components on actors hidden in game, and every controller hides itself.
	const AActor* Owner = PostProcess.GetOwner();
	if (Owner && Owner->IsHidden())
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s is on %s, which is hidden in game, as every controller is, so Unreal doesn't apply it. Leave Set Paused Post Process unset and use Project Settings > Plugins > Modular UI > Paused Look instead."),
			*GetPathNameSafe(&PostProcess), *GetNameSafe(Owner));
	}

	if (PausedBlendWeight <= 0.0f)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s has a Blend Weight of 0, so the paused look never shows. Set its Blend Weight to how strong the look should get, such as 1."),
			*GetPathNameSafe(&PostProcess));
	}

	// Without Infinite Extent, a post process inside a shape, such as a box, only shows while the camera is in that shape.
	if (!PostProcess.bUnbound && Cast<UShapeComponent>(PostProcess.GetAttachParent()))
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s is attached to a shape without Infinite Extent (Unbound), so it only shows while the camera is inside that shape."),
			*GetPathNameSafe(&PostProcess));
	}

	static const FPostProcessSettings DefaultSettings;
	if (FPostProcessSettings::StaticStruct()->CompareScriptStruct(&PostProcess.Settings, &DefaultSettings, PPF_None))
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s overrides no settings, so the paused look changes nothing. Tick the settings it should change, such as Color Grading > Global > Saturation."),
			*GetPathNameSafe(&PostProcess));
	}
}

void UModularUIPauseSubsystem::SetPausedLookForced(bool bForced)
{
	if (bForced != bPausedLookForced)
	{
		bPausedLookForced = bForced;
		StartFade();
	}
}

void UModularUIPauseSubsystem::StartFade()
{
	// Turns around mid-fade when the pause does. A fade time of 0 switches at once, without the ticker.
	if (!FadeHandle.IsValid() && TickFade(0.0f))
	{
		FadeHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickFade));
	}
}

bool UModularUIPauseSubsystem::TickFade(float DeltaTime)
{
	const float Target = bPauseRequested || bPausedLookForced ? 1.0f : 0.0f;
	const float FadeTime = Target > FadeAlpha ? GetFadeInTime() : GetFadeOutTime();
	FadeAlpha = FadeTime > 0.0f ? FMath::FInterpConstantTo(FadeAlpha, Target, DeltaTime, 1.0f / FadeTime) : Target;

	// Nothing shows the look, so there's nothing to fade.
	const bool bFading = ApplyFade() && FadeAlpha != Target;
	if (!bFading)
	{
		FadeHandle = FTSTicker::FDelegateHandle();
	}
	return bFading;
}

bool UModularUIPauseSubsystem::ApplyFade()
{
	const bool bShown = FadeAlpha > 0.0f;
	if (UPostProcessComponent* PostProcess = PausedPostProcess.Get())
	{
		PostProcess->BlendWeight = PausedBlendWeight * FadeAlpha;
		PostProcess->bEnabled = bShown;
		return true;
	}

	// Spawned only once the look is first wanted, so a game without a Paused Look, or that never pauses, never has the volume. Wanted, not
	// shown: a fade in starts at 0, and needs the volume to fade.
	const bool bWanted = bPauseRequested || bPausedLookForced;
	APostProcessVolume* Volume = bShown || bWanted ? FindOrSpawnPausedLookVolume() : PausedLookVolume.Get();
	if (Volume)
	{
		Volume->BlendWeight = GetDefault<UModularUISettings>()->PausedLookWeight * FadeAlpha;
		Volume->bEnabled = bShown;
		return true;
	}
	return false;
}

float UModularUIPauseSubsystem::GetFadeInTime() const
{
	return PausedPostProcess.IsValid() ? FadeInSeconds : FMath::Max(0.0f, GetDefault<UModularUISettings>()->PausedLookFadeInTime);
}

float UModularUIPauseSubsystem::GetFadeOutTime() const
{
	return PausedPostProcess.IsValid() ? FadeOutSeconds : FMath::Max(0.0f, GetDefault<UModularUISettings>()->PausedLookFadeOutTime);
}

APostProcessVolume* UModularUIPauseSubsystem::FindOrSpawnPausedLookVolume()
{
	if (APostProcessVolume* Existing = PausedLookVolume.Get())
	{
		return Existing;
	}

	const UModularUISettings* Settings = GetDefault<UModularUISettings>();
	UWorld* OwningWorld = GetWorld();
	if (!OwningWorld || !Settings->HasPausedLook())
	{
		return nullptr;
	}

	// A volume rather than a component, as Unreal applies volumes whether or not their actor is hidden. Transient, so it's never saved.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APostProcessVolume* Volume = OwningWorld->SpawnActor<APostProcessVolume>(SpawnParameters);
	if (!Volume)
	{
		return nullptr;
	}

	Volume->Settings = Settings->PausedLook;
	Volume->bUnbound = true;
	Volume->bEnabled = false;
	Volume->BlendWeight = 0.0f;
	Volume->Priority = Settings->PausedLookPriority;

	// The world orders volumes by Priority as each registers, which this did before it had its own, so it registers again with it.
	Volume->ReregisterAllComponents();

	PausedLookVolume = Volume;
	UE_LOG(LogModularUI, Log, TEXT("[Modular UI] Showing Project Settings' Paused Look with %s, at Priority %.1f."), *GetPathNameSafe(Volume), Volume->Priority);
	return Volume;
}

void UModularUIPauseSubsystem::LogStatus()
{
	const UWorld* PausedWorld = GetWorld();
	UE_LOG(LogModularUI, Display, TEXT("[Modular UI] Pause requested: %s. World paused: %s. Look forced: %s. %d request(s):"),
		bPauseRequested ? TEXT("yes") : TEXT("no"), PausedWorld && PausedWorld->IsPaused() ? TEXT("yes") : TEXT("no"), bPausedLookForced ? TEXT("yes") : TEXT("no"), PauseRequests.Num());
	for (const TWeakObjectPtr<const UObject>& Requester : PauseRequests)
	{
		UE_LOG(LogModularUI, Display, TEXT("[Modular UI]   %s"), *GetPathNameSafe(Requester.Get()));
	}

	if (const UPostProcessComponent* PostProcess = PausedPostProcess.Get())
	{
		UE_LOG(LogModularUI, Display, TEXT("[Modular UI] Paused post process %s: registered %s, Enabled %s, Blend Weight %.2f of %.2f (%.0f%% faded in), Priority %.1f, Infinite Extent %s."),
			*GetPathNameSafe(PostProcess), PostProcess->IsRegistered() ? TEXT("yes") : TEXT("no"), PostProcess->bEnabled ? TEXT("yes") : TEXT("no"),
			PostProcess->BlendWeight, PausedBlendWeight, FadeAlpha * 100.0f, PostProcess->Priority, PostProcess->bUnbound ? TEXT("yes") : TEXT("no"));
		WarnIfNothingShows(*PostProcess);
		return;
	}

	const UModularUISettings* Settings = GetDefault<UModularUISettings>();
	if (!Settings->HasPausedLook())
	{
		UE_LOG(LogModularUI, Display, TEXT("[Modular UI] No paused look: Project Settings > Plugins > Modular UI > Paused Look ticks no settings, and none was given to Set Paused Post Process."));
		return;
	}

	const APostProcessVolume* Volume = PausedLookVolume.Get();
	const FString VolumeState = Volume
		? FString::Printf(TEXT("%s is Enabled %s at Blend Weight %.2f"), *GetPathNameSafe(Volume), Volume->bEnabled ? TEXT("yes") : TEXT("no"), Volume->BlendWeight)
		: FString(TEXT("is spawned the first time the look shows"));
	UE_LOG(LogModularUI, Display, TEXT("[Modular UI] Paused Look from Project Settings: %.0f%% faded in, Weight %.2f, Priority %.1f. Its volume %s."),
		FadeAlpha * 100.0f, Settings->PausedLookWeight, Settings->PausedLookPriority, *VolumeState);
}
