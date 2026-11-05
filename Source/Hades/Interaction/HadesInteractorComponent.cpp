#include "HadesInteractorComponent.h"

#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

#include "Kismet/KismetSystemLibrary.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "HAL/IConsoleManager.h"

#include "TimerManager.h"

#include "Hades/HadesCollisionChannels.h"
#include "Hades/Camera/HadesCameraLibrary.h"
#include "Hades/Interaction/HadesInteractableComponent.h"
#include "Hades/UI/WorldMarker/HadesWorldMarker.h"
#include "Hades/UI/WorldMarker/HadesWorldMarkerManagerComponent.h"

namespace
{
	bool bShowNearbyDebug = false;
	FAutoConsoleVariableRef CVarShowNearby(TEXT("Hades.Interaction.ShowNearby"), bShowNearbyDebug,
		TEXT("Draw the nearby radius."), ECVF_Cheat);

	bool bShowFocusDebug = false;
	FAutoConsoleVariableRef CVarShowFocus(TEXT("Hades.Interaction.ShowFocus"), bShowFocusDebug,
		TEXT("Draw the focus trace."), ECVF_Cheat);

	bool bShowOcclusionDebug = false;
	FAutoConsoleVariableRef CVarShowOcclusion(TEXT("Hades.Interaction.ShowOcclusion"), bShowOcclusionDebug,
		TEXT("Draw the occlusion check for each nearby interactable. Red when occluded."), ECVF_Cheat);

	float NearbyRadiusOverride = -1.0f;
	FAutoConsoleVariableRef CVarNearbyRadiusOverride(TEXT("Hades.Interaction.NearbyRadiusOverride"), NearbyRadiusOverride,
		TEXT("Override the nearby radius. -1 uses the component's value."), ECVF_Cheat);

	float FocusDistanceOverride = -1.0f;
	FAutoConsoleVariableRef CVarFocusDistanceOverride(TEXT("Hades.Interaction.FocusDistanceOverride"), FocusDistanceOverride,
		TEXT("Override the focus distance. -1 uses the component's value."), ECVF_Cheat);

	// Only focus targets block the Interaction channel, and each belongs to exactly one interactable.
	UHadesInteractableComponent* FindInteractableWithFocusTarget(const UPrimitiveComponent* Component)
	{
		AActor* Owner = Component ? Component->GetOwner() : nullptr;
		if (!IsValid(Owner))
		{
			return nullptr;
		}

		TInlineComponentArray<UHadesInteractableComponent*> Interactables(Owner);
		for (UHadesInteractableComponent* Interactable : Interactables)
		{
			if (Interactable->GetFocusTarget() == Component)
			{
				return Interactable;
			}
		}
		return nullptr;
	}

	// Through the marker rather than the controller's manager, which may already be gone when the pawn is unpossessed.
	void UnregisterWorldMarker(const UHadesInteractableComponent& Interactable)
	{
		if (UHadesWorldMarker* Marker = Interactable.GetWorldMarker())
		{
			Marker->Unregister();
		}
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

	StartNearbyUpdateTimer();
}

void UHadesInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NearbyUpdateTimerHandle);
	}
	ClearInteractables();

	Super::EndPlay(EndPlayReason);
}

void UHadesInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UHadesInteractableComponent* NewFocused = TraceForInteractable();
	if (!NewFocused && CanKeepFocus())
	{
		NewFocused = FocusedInteractable.Get();
	}
	SetFocusedInteractable(NewFocused);
}

void UHadesInteractorComponent::StartInteraction()
{
	if (UHadesInteractableComponent* Interactable = FocusedInteractable.Get())
	{
		Interactable->TryStartInteraction(GetOwner());
	}
}

void UHadesInteractorComponent::StopInteraction()
{
	if (UHadesInteractableComponent* Interactable = FocusedInteractable.Get())
	{
		Interactable->TryStopInteraction(GetOwner());
	}
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
		StartNearbyUpdateTimer();
	}
	else
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(NearbyUpdateTimerHandle);
		}
		ClearInteractables();
	}
}

void UHadesInteractorComponent::StartNearbyUpdateTimer()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			NearbyUpdateTimerHandle,
			this,
			&ThisClass::UpdateNearbyInteractables,
			NearbyUpdateInterval,
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

	UHadesWorldMarkerManagerComponent* MarkerManager = UHadesWorldMarkerManagerComponent::Get(Pawn->GetController());

	const float Radius = NearbyRadiusOverride >= 0.0f ? NearbyRadiusOverride : NearbyRadius;
	const FVector Location = Pawn->GetActorLocation();

	// An overlap rather than a multi trace: a multi trace stops at the first blocking hit, and every focus target blocks this channel.
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HadesNearbyInteractables), false, Pawn);
	GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, HadesCollisionChannels::Interaction,
		FCollisionShape::MakeSphere(Radius), Params);

	if (bShowNearbyDebug)
	{
		UKismetSystemLibrary::DrawDebugSphere(this, Location, Radius, 24, FLinearColor::Green, NearbyUpdateInterval);
	}

	TSet<TWeakObjectPtr<UHadesInteractableComponent>> NewNearby;
	NewNearby.Reserve(Overlaps.Num());

	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (UHadesInteractableComponent* Interactable = FindInteractableWithFocusTarget(Overlap.GetComponent()))
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
			Interactable->UpdateOcclusion(ViewLocation, Pawn);

			if (bShowOcclusionDebug)
			{
				UKismetSystemLibrary::DrawDebugLine(this, ViewLocation, Interactable->GetMarkerLocation(),
					Interactable->IsOccluded() ? FLinearColor::Red : FLinearColor::Green, NearbyUpdateInterval);
			}
		}
	}

	for (const TWeakObjectPtr<UHadesInteractableComponent>& Interactable : OldNearby)
	{
		if (Interactable.IsValid() && !NewNearby.Contains(Interactable))
		{
			UnregisterWorldMarker(*Interactable);
			Interactable->OnNearbyEnd();
		}
	}

	for (const TWeakObjectPtr<UHadesInteractableComponent>& Interactable : NewNearby)
	{
		if (Interactable.IsValid() && !OldNearby.Contains(Interactable))
		{
			if (MarkerManager)
			{
				MarkerManager->AddMarker(Interactable->GetWorldMarker());
			}
			Interactable->OnNearbyBegin();
		}
	}
}

UHadesInteractableComponent* UHadesInteractorComponent::TraceForInteractable() const
{
	FHitResult Hit;
	const bool bHit = UHadesCameraLibrary::CameraForwardTrace(
		GetOwner<APawn>(),
		GetFocusDistance(),
		UEngineTypes::ConvertToTraceType(HadesCollisionChannels::Interaction),
		Hit,
		false,
		0.0f,
		bShowFocusDebug ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None);

	UHadesInteractableComponent* Interactable = bHit ? FindInteractableWithFocusTarget(Hit.GetComponent()) : nullptr;
	if (!Interactable || !NearbyInteractables.Contains(Interactable) || Interactable->IsOccluded())
	{
		return nullptr;
	}
	return Interactable;
}

bool UHadesInteractorComponent::CanKeepFocus() const
{
	const UHadesInteractableComponent* Interactable = FocusedInteractable.Get();
	const UPrimitiveComponent* FocusTarget = Interactable ? Interactable->GetFocusTarget() : nullptr;
	if (!FocusTarget || FocusKeepAngle <= 0.0f || Interactable->IsOccluded())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	UHadesCameraLibrary::GetCameraViewPoint(GetOwner<APawn>(), ViewLocation, ViewRotation);

	const FBox Bounds = FocusTarget->Bounds.GetBox();
	if (FVector::DistSquared(ViewLocation, Bounds.GetClosestPointTo(ViewLocation)) > FMath::Square(GetFocusDistance()))
	{
		return false;
	}

	const FVector ToTarget = (Bounds.GetCenter() - ViewLocation).GetSafeNormal();
	return FVector::DotProduct(ViewRotation.Vector(), ToTarget) >= FMath::Cos(FMath::DegreesToRadians(FocusKeepAngle));
}

float UHadesInteractorComponent::GetFocusDistance() const
{
	return FocusDistanceOverride >= 0.0f ? FocusDistanceOverride : FocusDistance;
}

void UHadesInteractorComponent::SetFocusedInteractable(UHadesInteractableComponent* NewFocused)
{
	UHadesInteractableComponent* OldFocused = FocusedInteractable.Get();
	if (OldFocused == NewFocused && !FocusedInteractable.IsStale())
	{
		return;
	}

	FocusedInteractable = NewFocused;

	if (OldFocused)
	{
		OldFocused->TryStopInteraction(GetOwner());
		OldFocused->OnFocusEnd();
	}

	if (NewFocused)
	{
		NewFocused->OnFocusBegin();
	}

	OnFocusedInteractableChanged.Broadcast(NewFocused);
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
			UnregisterWorldMarker(*Interactable);
			Interactable->OnNearbyEnd();
		}
	}
}
