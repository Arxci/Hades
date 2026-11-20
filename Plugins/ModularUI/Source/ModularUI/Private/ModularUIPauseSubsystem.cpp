#include "ModularUIPauseSubsystem.h"

#include "Components/PostProcessComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "ModularUI.h"

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

	UE_LOG(LogModularUI, Log, TEXT("[Modular UI] World %s. Paused post process: %s, fading to a Blend Weight of %.2f."),
		bWantsPause ? TEXT("paused") : TEXT("resumed"), *GetPathNameSafe(PausedPostProcess.Get()), bWantsPause ? PausedBlendWeight : 0.0f);
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

		// The fade drives Blend Weight from here, up to whatever the component was given.
		PausedPostProcess = PostProcess;
		FadeAlpha = 0.0f;
		if (PostProcess)
		{
			PausedBlendWeight = PostProcess->BlendWeight;
			PostProcess->BlendWeight = 0.0f;
			PostProcess->bEnabled = false;
		}
	}
	StartFade();
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
	UPostProcessComponent* PostProcess = PausedPostProcess.Get();
	if (!PostProcess)
	{
		FadeHandle = FTSTicker::FDelegateHandle();
		return false;
	}

	const float Target = bPauseRequested ? 1.0f : 0.0f;
	const float FadeTime = Target > FadeAlpha ? FadeInSeconds : FadeOutSeconds;
	FadeAlpha = FadeTime > 0.0f ? FMath::FInterpConstantTo(FadeAlpha, Target, DeltaTime, 1.0f / FadeTime) : Target;
	PostProcess->BlendWeight = PausedBlendWeight * FadeAlpha;
	PostProcess->bEnabled = FadeAlpha > 0.0f;

	const bool bFading = FadeAlpha != Target;
	if (!bFading)
	{
		FadeHandle = FTSTicker::FDelegateHandle();
	}
	return bFading;
}
