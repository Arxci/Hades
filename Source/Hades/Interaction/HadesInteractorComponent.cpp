#include "HadesInteractorComponent.h"

#include "Engine/World.h"

#include "Kismet/KismetSystemLibrary.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "HAL/IConsoleManager.h"

#include "TimerManager.h"

#include "Hades/HadesCollisionChannels.h"
#include "Hades/Camera/HadesCameraLibrary.h"
#include "Hades/Interaction/HadesInteractableComponent.h"

namespace
{
	bool bShowAwarenessDebug = false;
	FAutoConsoleVariableRef CVarShowAwareness(TEXT("Hades.Interaction.ShowAwareness"), bShowAwarenessDebug,
		TEXT("Draw the awareness radius and the interactables in range."), ECVF_Cheat);

	bool bShowFocusDebug = false;
	FAutoConsoleVariableRef CVarShowFocus(TEXT("Hades.Interaction.ShowFocus"), bShowFocusDebug,
		TEXT("Draw the focus trace."), ECVF_Cheat);

	bool bShowOcclusionDebug = false;
	FAutoConsoleVariableRef CVarShowOcclusion(TEXT("Hades.Interaction.ShowOcclusion"), bShowOcclusionDebug,
		TEXT("Draw the occlusion check for each interactable in range. Red when occluded."), ECVF_Cheat);

	float AwarenessRadiusOverride = -1.0f;
	FAutoConsoleVariableRef CVarAwarenessRadiusOverride(TEXT("Hades.Interaction.AwarenessRadiusOverride"), AwarenessRadiusOverride,
		TEXT("Override the awareness radius. -1 uses the component's value."), ECVF_Cheat);

	float FocusDistanceOverride = -1.0f;
	FAutoConsoleVariableRef CVarFocusDistanceOverride(TEXT("Hades.Interaction.FocusDistanceOverride"), FocusDistanceOverride,
		TEXT("Override the focus distance. -1 uses the component's value."), ECVF_Cheat);

	// Only focus targets block the Interaction channel, so each hit is the focus target of exactly one interactable.
	UHadesInteractableComponent* FindInteractableWithFocusTarget(const FHitResult& Hit)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
		{
			return nullptr;
		}

		TInlineComponentArray<UHadesInteractableComponent*> Interactables(HitActor);
		for (UHadesInteractableComponent* Interactable : Interactables)
		{
			if (Interactable->GetFocusTarget() == Hit.GetComponent())
			{
				return Interactable;
			}
		}
		return nullptr;
	}
}

UHadesInteractorComponent::UHadesInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHadesInteractorComponent::BeginPlay()
{
	Super::BeginPlay();

	StartAwarenessTimer();
}

void UHadesInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AwarenessTimerHandle);
	}
	ClearInteractables();

	Super::EndPlay(EndPlayReason);
}

void UHadesInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SetFocusedInteractable(TraceForInteractable());
}

void UHadesInteractorComponent::SetInteractionEnabled(bool bEnabled)
{
	if (bInteractionEnabled == bEnabled)
	{
		return;
	}

	bInteractionEnabled = bEnabled;

	if (bEnabled)
	{
		StartAwarenessTimer();
	}
	else
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AwarenessTimerHandle);
		}
		ClearInteractables();
	}
}

void UHadesInteractorComponent::StartAwarenessTimer()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AwarenessTimerHandle,
			this,
			&ThisClass::UpdateNearbyInteractables,
			AwarenessInterval,
			true);
	}
}

void UHadesInteractorComponent::UpdateNearbyInteractables()
{
	const APawn* Pawn = GetOwner<APawn>();
	if (!Pawn || !Pawn->GetController<APlayerController>())
	{
		ClearInteractables();
		return;
	}

	const float Radius = AwarenessRadiusOverride >= 0.0f ? AwarenessRadiusOverride : AwarenessRadius;
	const FVector Location = Pawn->GetActorLocation();

	TArray<FHitResult> Hits;
	UKismetSystemLibrary::SphereTraceMulti(
		Pawn,
		Location,
		Location,
		Radius,
		UEngineTypes::ConvertToTraceType(HadesCollisionChannels::Interaction),
		false,
		{},
		bShowAwarenessDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		Hits,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		AwarenessInterval
	);

	TSet<TWeakObjectPtr<UHadesInteractableComponent>> NewNearby;
	NewNearby.Reserve(Hits.Num());

	for (const FHitResult& Hit : Hits)
	{
		if (UHadesInteractableComponent* Interactable = FindInteractableWithFocusTarget(Hit))
		{
			NewNearby.Add(Interactable);
		}
	}

	if (!NewNearby.Contains(FocusedInteractable))
	{
		SetFocusedInteractable(nullptr);
	}

	// Notify from local copies: a state change can run Blueprint that destroys actors or this pawn.
	const TSet<TWeakObjectPtr<UHadesInteractableComponent>> OldNearby = MoveTemp(NearbyInteractables);
	NearbyInteractables = NewNearby;
	SetComponentTickEnabled(!NearbyInteractables.IsEmpty());

	FVector ViewLocation;
	FRotator ViewRotation;
	UHadesCameraLibrary::GetCameraViewPoint(Pawn, ViewLocation, ViewRotation);

	for (const TWeakObjectPtr<UHadesInteractableComponent>& WeakInteractable : NewNearby)
	{
		if (UHadesInteractableComponent* Interactable = WeakInteractable.Get())
		{
			Interactable->UpdateOcclusion(ViewLocation);

			if (bShowOcclusionDebug)
			{
				UKismetSystemLibrary::DrawDebugLine(this, ViewLocation, Interactable->GetInteractionLocation(),
					Interactable->IsOccluded() ? FLinearColor::Red : FLinearColor::Green, AwarenessInterval);
			}
		}
	}

	for (const TWeakObjectPtr<UHadesInteractableComponent>& Interactable : OldNearby)
	{
		if (Interactable.IsValid() && !NewNearby.Contains(Interactable))
		{
			Interactable->OnLeftPlayerRange();
		}
	}

	for (const TWeakObjectPtr<UHadesInteractableComponent>& Interactable : NewNearby)
	{
		if (Interactable.IsValid() && !OldNearby.Contains(Interactable))
		{
			Interactable->OnEnteredPlayerRange();
		}
	}
}

UHadesInteractableComponent* UHadesInteractorComponent::TraceForInteractable() const
{
	const float Distance = FocusDistanceOverride >= 0.0f ? FocusDistanceOverride : FocusDistance;

	FHitResult Hit;
	const bool bHit = UHadesCameraLibrary::CameraForwardTrace(
		GetOwner<APawn>(),
		Distance,
		UEngineTypes::ConvertToTraceType(HadesCollisionChannels::Interaction),
		Hit,
		false,
		0.0f,
		bShowFocusDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None);

	UHadesInteractableComponent* Interactable = bHit ? FindInteractableWithFocusTarget(Hit) : nullptr;
	if (!Interactable || !NearbyInteractables.Contains(Interactable) || Interactable->IsOccluded())
	{
		return nullptr;
	}
	return Interactable;
}

void UHadesInteractorComponent::SetFocusedInteractable(UHadesInteractableComponent* NewFocused)
{
	UHadesInteractableComponent* OldFocused = FocusedInteractable.Get();
	const bool bFocusedWasDestroyed = FocusedInteractable.IsStale();
	if (OldFocused == NewFocused && !bFocusedWasDestroyed)
	{
		return;
	}

	FocusedInteractable = NewFocused;

	if (OldFocused)
	{
		OldFocused->OnLostPlayerFocus();
	}
	else if (bFocusedWasDestroyed)
	{
		OnInteractionFocusCleared.Broadcast();
	}

	if (NewFocused)
	{
		NewFocused->OnGainedPlayerFocus();
	}
}

void UHadesInteractorComponent::ClearInteractables()
{
	SetFocusedInteractable(nullptr);

	const TSet<TWeakObjectPtr<UHadesInteractableComponent>> OldNearby = MoveTemp(NearbyInteractables);
	NearbyInteractables.Reset();
	SetComponentTickEnabled(false);

	for (const TWeakObjectPtr<UHadesInteractableComponent>& Interactable : OldNearby)
	{
		if (Interactable.IsValid())
		{
			Interactable->OnLeftPlayerRange();
		}
	}
}
