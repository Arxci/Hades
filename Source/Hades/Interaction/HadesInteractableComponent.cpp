#include "HadesInteractableComponent.h"

#include "Algo/AnyOf.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Hades/Hades.h"
#include "Hades/HadesCollisionChannels.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/UI/WorldMarker/HadesWorldMarker.h"

UHadesInteractableComponent::UHadesInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHadesInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveFocusComponent();
	ResolveOcclusionComponents();
	ResolveMarkerComponent();

	SetStateTag(HadesGameplayTags::Interactable_State_Disabled, !bInteractionEnabled);
	InitializeWorldMarker();
}

void UHadesInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (WorldMarker)
	{
		WorldMarker->Unregister();
		WorldMarker = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UHadesInteractableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CurrentInteractor)
	{
		return;
	}

	const float ElapsedTime = GetElapsedTime();
	OnInteractionProgressed.Broadcast(ElapsedTime);

	if (WorldMarker)
	{
		WorldMarker->SetProgress(FMath::Clamp(ElapsedTime / InteractionDuration, 0.0f, 1.0f));
	}

	if (ElapsedTime >= InteractionDuration)
	{
		CompleteInteraction();
	}
}

//Interaction
void UHadesInteractableComponent::TryStartInteraction_Implementation(AActor* Interactor)
{
	if (CurrentInteractor || !bInteractionEnabled)
	{
		return;
	}

	CurrentInteractor = Interactor;
	InteractionStartTime = GetWorld()->GetTimeSeconds();
	SetStateTag(HadesGameplayTags::Interactable_State_Interacting, true);
	OnInteractionStarted.Broadcast(Interactor);

	UE_LOG(LogHadesInteraction, Verbose, TEXT("[Interact] %s started interaction on %s (duration=%.2fs)."),
		*GetNameSafe(Interactor), *GetNameSafe(GetOwner()), InteractionDuration);

	if (InteractionDuration <= 0.0f)
	{
		CompleteInteraction();
		return;
	}

	SetComponentTickEnabled(true);
}

void UHadesInteractableComponent::TryStopInteraction_Implementation(AActor* Interactor)
{
	if (CurrentInteractor && CurrentInteractor == Interactor)
	{
		CancelInteraction();
	}
}

void UHadesInteractableComponent::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	SetStateTag(HadesGameplayTags::Interactable_State_Disabled, !bEnabled);

	if (!bEnabled && CurrentInteractor)
	{
		CancelInteraction();
	}
}

float UHadesInteractableComponent::GetElapsedTime() const
{
	return CurrentInteractor ? GetWorld()->GetTimeSeconds() - InteractionStartTime : 0.0f;
}

void UHadesInteractableComponent::SetInteractionText(const FText& NewText)
{
	InteractionText = NewText;

	if (WorldMarker)
	{
		WorldMarker->RefreshWidget();
	}
}

void UHadesInteractableComponent::SetDisabledText(const FText& NewText)
{
	DisabledText = NewText;

	if (WorldMarker)
	{
		WorldMarker->RefreshWidget();
	}
}

void UHadesInteractableComponent::CompleteInteraction()
{
	AActor* Interactor = ResetInteraction(HadesGameplayTags::Interactable_Event_Completed);

	UE_LOG(LogHadesInteraction, Verbose, TEXT("[Interact] %s completed interaction on %s."),
		*GetNameSafe(Interactor), *GetNameSafe(GetOwner()));

	OnInteractionCompleted.Broadcast(Interactor);
	OnInteractionEnded.Broadcast(Interactor);
}

void UHadesInteractableComponent::CancelInteraction()
{
	AActor* Interactor = ResetInteraction(HadesGameplayTags::Interactable_Event_Cancelled);

	UE_LOG(LogHadesInteraction, Verbose, TEXT("[Interact] %s cancelled interaction on %s."),
		*GetNameSafe(Interactor), *GetNameSafe(GetOwner()));

	OnInteractionCancelled.Broadcast(Interactor);
	OnInteractionEnded.Broadcast(Interactor);
}

AActor* UHadesInteractableComponent::ResetInteraction(const FGameplayTag& MarkerEvent)
{
	AActor* Interactor = CurrentInteractor;
	CurrentInteractor = nullptr;
	SetComponentTickEnabled(false);

	if (WorldMarker)
	{
		WorldMarker->NotifyEvent(MarkerEvent);
		WorldMarker->SetProgress(0.0f);
	}
	SetStateTag(HadesGameplayTags::Interactable_State_Interacting, false);

	return Interactor;
}

//Nearby and focus
void UHadesInteractableComponent::OnNearbyBegin_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_Nearby, true);
}

void UHadesInteractableComponent::OnNearbyEnd_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_Nearby, false);
}

void UHadesInteractableComponent::OnFocusBegin_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_Focused, true);
}

void UHadesInteractableComponent::OnFocusEnd_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_Focused, false);
}

void UHadesInteractableComponent::UpdateOcclusion(const FVector& ViewLocation, const AActor* Viewer)
{
	// Traced from the view toward the marker so a marker inside an occluder's collision is still caught.
	const FVector MarkerLocation = GetMarkerLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HadesInteractableOcclusion), false, GetOwner());
	Params.AddIgnoredActor(Viewer);

	const bool bOccluded = GetWorld()->LineTraceTestByChannel(ViewLocation, MarkerLocation, ECC_Visibility, Params)
		|| Algo::AnyOf(OcclusionTargets, [&](UPrimitiveComponent* Occluder)
		{
			FHitResult Hit;
			return Occluder && Occluder->LineTraceComponent(Hit, ViewLocation, MarkerLocation, FCollisionQueryParams::DefaultQueryParam);
		});

	SetStateTag(HadesGameplayTags::Interactable_State_Occluded, bOccluded);
}

bool UHadesInteractableComponent::IsOccluded() const
{
	return HasStateTag(HadesGameplayTags::Interactable_State_Occluded);
}

//Resolve
void UHadesInteractableComponent::ResolveFocusComponent()
{
	FocusTarget = FocusComponent.GetComponent<UPrimitiveComponent>(this);
	if (!FocusTarget)
	{
		FocusTarget = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	}
	if (!FocusTarget)
	{
		return;
	}

	// The Interaction channel ignores everything by default, so only focus targets are found by the interactor's traces.
	FocusTarget->SetCollisionResponseToChannel(HadesCollisionChannels::Interaction, ECR_Block);
	if (FocusTarget->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		FocusTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void UHadesInteractableComponent::ResolveOcclusionComponents()
{
	for (FComponentPicker& Picker : OcclusionComponents)
	{
		if (UPrimitiveComponent* Occluder = Picker.GetComponent<UPrimitiveComponent>(this))
		{
			OcclusionTargets.AddUnique(Occluder);
		}
	}
}

void UHadesInteractableComponent::ResolveMarkerComponent()
{
	MarkerAnchor = MarkerComponent.GetComponent<USceneComponent>(this);
	if (!MarkerAnchor)
	{
		MarkerAnchor = GetOwner()->GetRootComponent();
	}
}

void UHadesInteractableComponent::InitializeWorldMarker()
{
	if (MarkerWidgetClass.IsNull() || !MarkerAnchor)
	{
		return;
	}

	// Registered with the player's marker manager by the interactor while nearby.
	WorldMarker = NewObject<UHadesWorldMarker>(this);
	WorldMarker->SetAnchor(MarkerAnchor);
	WorldMarker->SetWidgetClass(MarkerWidgetClass);
	WorldMarker->SetContextObject(this);
	WorldMarker->SetVisible(false);

	for (const FGameplayTag& Tag : StateTags)
	{
		WorldMarker->SetStateTag(Tag, true);
	}
}

//Utility
FVector UHadesInteractableComponent::GetMarkerLocation() const
{
	return MarkerAnchor ? MarkerAnchor->GetComponentLocation() : GetOwner()->GetActorLocation();
}

void UHadesInteractableComponent::UpdateMarkerVisibility()
{
	if (WorldMarker)
	{
		const bool bVisible = HasStateTag(HadesGameplayTags::Interactable_State_Nearby) && !IsOccluded()
			&& (bShowMarkerWhenNearby || HasStateTag(HadesGameplayTags::Interactable_State_Focused));
		WorldMarker->SetVisible(bVisible);
	}
}

void UHadesInteractableComponent::SetStateTag(const FGameplayTag& Tag, bool bEnabled)
{
	if (StateTags.HasTagExact(Tag) == bEnabled)
	{
		return;
	}

	if (bEnabled)
	{
		StateTags.AddTag(Tag);
	}
	else
	{
		StateTags.RemoveTag(Tag);
	}

	if (WorldMarker)
	{
		WorldMarker->SetStateTag(Tag, bEnabled);
	}
	UpdateMarkerVisibility();

	OnStateTagChanged.Broadcast(Tag, bEnabled);
}
