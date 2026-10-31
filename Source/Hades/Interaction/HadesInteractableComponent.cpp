#include "HadesInteractableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Hades/Hades.h"
#include "Hades/HadesCollisionChannels.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/UI/HadesWorldMarkerWidget.h"

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
	InitializeMarkerWidget();

	SetStateTag(HadesGameplayTags::Interactable_State_Disabled, !bInteractionEnabled);
}

void UHadesInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MarkerWidgetComponent)
	{
		MarkerWidgetComponent->DestroyComponent();
		MarkerWidgetComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UHadesInteractableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	InteractionProgress();
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

void UHadesInteractableComponent::InteractionProgress_Implementation()
{
	if (!CurrentInteractor)
	{
		return;
	}

	const float ElapsedTime = GetElapsedTime();
	OnInteractionProgressed.Broadcast(ElapsedTime);

	if (UHadesWorldMarkerWidget* Marker = GetMarkerWidget())
	{
		Marker->SetProgress(FMath::Clamp(ElapsedTime / InteractionDuration, 0.0f, 1.0f));
	}

	if (ElapsedTime >= InteractionDuration)
	{
		CompleteInteraction();
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

void UHadesInteractableComponent::CompleteInteraction()
{
	AActor* Interactor = ResetInteraction();

	UE_LOG(LogHadesInteraction, Verbose, TEXT("[Interact] %s completed interaction on %s."),
		*GetNameSafe(Interactor), *GetNameSafe(GetOwner()));

	OnInteractionCompleted.Broadcast(Interactor);
	OnInteractionEnded.Broadcast(Interactor);
}

void UHadesInteractableComponent::CancelInteraction()
{
	AActor* Interactor = ResetInteraction();

	UE_LOG(LogHadesInteraction, Verbose, TEXT("[Interact] %s cancelled interaction on %s."),
		*GetNameSafe(Interactor), *GetNameSafe(GetOwner()));

	OnInteractionCancelled.Broadcast(Interactor);
	OnInteractionEnded.Broadcast(Interactor);
}

AActor* UHadesInteractableComponent::ResetInteraction()
{
	AActor* Interactor = CurrentInteractor;
	CurrentInteractor = nullptr;
	SetComponentTickEnabled(false);

	if (UHadesWorldMarkerWidget* Marker = GetMarkerWidget())
	{
		Marker->SetProgress(0.0f);
	}
	SetStateTag(HadesGameplayTags::Interactable_State_Interacting, false);

	return Interactor;
}

//Awareness
void UHadesInteractableComponent::OnEnteredPlayerRange_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_InRange, true);
	UpdateMarkerVisibility();
}

void UHadesInteractableComponent::OnLeftPlayerRange_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_InRange, false);
	UpdateMarkerVisibility();
}

void UHadesInteractableComponent::OnGainedPlayerFocus_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_Focused, true);
}

void UHadesInteractableComponent::OnLostPlayerFocus_Implementation()
{
	SetStateTag(HadesGameplayTags::Interactable_State_Focused, false);
}

void UHadesInteractableComponent::UpdateOcclusion(const FVector& ViewLocation)
{
	if (OcclusionTargets.IsEmpty())
	{
		return;
	}

	// Traced from the view toward the marker so a marker inside an occluder's collision is still caught.
	const FVector MarkerLocation = GetInteractionLocation();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HadesInteractableOcclusion));

	bool bOccluded = false;
	for (UPrimitiveComponent* Occluder : OcclusionTargets)
	{
		FHitResult Hit;
		if (Occluder && Occluder->LineTraceComponent(Hit, ViewLocation, MarkerLocation, Params))
		{
			bOccluded = true;
			break;
		}
	}

	SetStateTag(HadesGameplayTags::Interactable_State_Occluded, bOccluded);
	UpdateMarkerVisibility();
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
	MarkerTarget = MarkerComponent.GetComponent<USceneComponent>(this);
	if (!MarkerTarget)
	{
		MarkerTarget = GetOwner()->GetRootComponent();
	}
}

void UHadesInteractableComponent::InitializeMarkerWidget()
{
	if (!MarkerWidgetClass || !MarkerTarget)
	{
		return;
	}

	MarkerWidgetComponent = NewObject<UWidgetComponent>(GetOwner(), NAME_None, RF_Transient);
	MarkerWidgetComponent->SetupAttachment(MarkerTarget);
	MarkerWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	MarkerWidgetComponent->SetDrawAtDesiredSize(true);
	MarkerWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerWidgetComponent->SetWidgetClass(MarkerWidgetClass);
	MarkerWidgetComponent->SetVisibility(false);
	MarkerWidgetComponent->PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	MarkerWidgetComponent->RegisterComponent();
	MarkerWidgetComponent->InitWidget();

	if (UHadesWorldMarkerWidget* Marker = GetMarkerWidget())
	{
		Marker->SetContext(MarkerTarget, GetOwner());
	}
}

//Utility
FVector UHadesInteractableComponent::GetInteractionLocation() const
{
	return MarkerTarget ? MarkerTarget->GetComponentLocation() : GetOwner()->GetActorLocation();
}

void UHadesInteractableComponent::UpdateMarkerVisibility()
{
	// Hidden screen-space widget components are taken off the screen, so hidden markers cost nothing to draw.
	if (MarkerWidgetComponent)
	{
		MarkerWidgetComponent->SetVisibility(HasStateTag(HadesGameplayTags::Interactable_State_InRange) && !IsOccluded());
	}
}

UHadesWorldMarkerWidget* UHadesInteractableComponent::GetMarkerWidget() const
{
	return MarkerWidgetComponent ? Cast<UHadesWorldMarkerWidget>(MarkerWidgetComponent->GetUserWidgetObject()) : nullptr;
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

	if (UHadesWorldMarkerWidget* Marker = GetMarkerWidget())
	{
		Marker->SetStateTag(Tag, bEnabled);
	}

	OnStateTagChanged.Broadcast(Tag, bEnabled);
}
