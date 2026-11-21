#include "HadesCameraModifier_AutoFocus.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace HadesAutoFocus
{
	static TAutoConsoleVariable<bool> CVarDebug(
		TEXT("Hades.AutoFocus.Debug"),
		false,
		TEXT("Marks the auto focus point with a yellow sphere and prints the focus distance and aperture."));

	/** An aperture that reads as no blur. Fading opens from here to the target aperture. */
	constexpr float ClosedAperture = 32.0f;
}

UHadesCameraModifier_AutoFocus::UHadesCameraModifier_AutoFocus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AlphaInTime = 0.3f;
	AlphaOutTime = 0.3f;
}

bool UHadesCameraModifier_AutoFocus::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	if (Alpha <= 0.0f || !IsViewingThroughPlayerPawn())
	{
		// Pick focus up again instantly next time, instead of racking from a stale distance.
		bHasFocus = false;
		return false;
	}

	const UWorld* World = GetWorld();
	if (bSkipWhilePaused && World && World->IsPaused())
	{
		return false;
	}

	const float TargetDiopters = 1.0f / TraceFocusDistance(InOutPOV);
	CurrentDiopters = bHasFocus ? FMath::FInterpTo(CurrentDiopters, TargetDiopters, DeltaTime, FocusSpeed) : TargetDiopters;
	bHasFocus = true;

	// The view's own settings are applied after every Post Process Volume and hold the camera component's values,
	// so writing here wins over a fixed Focal Distance anywhere else. A weight of 0 means the camera shares nothing;
	// start from empty settings so only these overrides apply.
	if (InOutPOV.PostProcessBlendWeight <= 0.0f)
	{
		InOutPOV.PostProcessSettings = FPostProcessSettings();
		InOutPOV.PostProcessBlendWeight = 1.0f;
	}

	FPostProcessSettings& Settings = InOutPOV.PostProcessSettings;
	Settings.bOverride_DepthOfFieldFocalDistance = true;
	Settings.DepthOfFieldFocalDistance = GetFocusDistance();

	if (bOverrideAperture)
	{
		// Fade the blur in and out by opening the aperture evenly in stops.
		Settings.bOverride_DepthOfFieldFstop = true;
		Settings.DepthOfFieldFstop = FMath::Exp(FMath::Lerp(FMath::Loge(HadesAutoFocus::ClosedAperture), FMath::Loge(Aperture), Alpha));
	}

#if ENABLE_DRAW_DEBUG
	if (HadesAutoFocus::CVarDebug.GetValueOnGameThread() && World)
	{
		const FVector FocusPoint = InOutPOV.Location + InOutPOV.Rotation.Vector() * GetFocusDistance();
		DrawDebugSphere(World, FocusPoint, 6.0f, 8, FColor::Yellow);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow,
				FString::Printf(TEXT("Auto focus %.0f cm (target %.0f cm), f/%.1f, blend %.2f"),
					GetFocusDistance(), 1.0f / TargetDiopters, Settings.DepthOfFieldFstop, Alpha));
		}
	}
#endif

	return false;
}

float UHadesCameraModifier_AutoFocus::GetTargetAlpha()
{
	if (!BlockedTags.IsEmpty())
	{
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetViewTarget());
		if (ASC && ASC->HasAnyMatchingGameplayTags(BlockedTags))
		{
			return 0.0f;
		}
	}

	return Super::GetTargetAlpha();
}

bool UHadesCameraModifier_AutoFocus::IsViewingThroughPlayerPawn() const
{
	const APlayerController* PlayerController = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	return Pawn && GetViewTarget() == Pawn;
}

float UHadesCameraModifier_AutoFocus::TraceFocusDistance(const FMinimalViewInfo& POV) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return MaxFocusDistance;
	}

	// Ignore the player and everything attached to them (held weapons, flashlights), so focus lands on the world.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HadesAutoFocus), false);
	if (AActor* ViewTarget = GetViewTarget())
	{
		Params.AddIgnoredActor(ViewTarget);

		TArray<AActor*> AttachedActors;
		ViewTarget->GetAttachedActors(AttachedActors, true, true);
		Params.AddIgnoredActors(AttachedActors);
	}

	const FVector Start = POV.Location;
	const FVector End = Start + POV.Rotation.Vector() * TraceDistance;

	FHitResult Hit;
	const bool bHit = TraceRadius > 0.0f
		? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TraceRadius), Params)
		: World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	// A sweep reports where the sphere's center stopped; add the radius to land on the surface.
	const float Distance = bHit ? Hit.Distance + TraceRadius : MaxFocusDistance;
	return FMath::Clamp(Distance, MinFocusDistance, MaxFocusDistance);
}
