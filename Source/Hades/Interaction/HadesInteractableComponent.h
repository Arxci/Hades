#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "ComponentPicker.h"
#include "GameplayTagContainer.h"

#include "HadesInteractableComponent.generated.h"

class UPrimitiveComponent;
class USceneComponent;
class UWidgetComponent;
class UHadesWorldMarkerWidget;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHadesInteractableStateTagChangedSignature, FGameplayTag, Tag, bool, bAdded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesInteractionProgressedSignature, float, ElapsedTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesInteractionSignature, AActor*, Interactor);


/** Makes its actor interactable and shows a world marker while the player is in range. */
UCLASS(Blueprintable, ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHadesInteractableComponent();

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// Interaction
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void TryStartInteraction(AActor* Interactor);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void TryStopInteraction(AActor* Interactor);

	/** A disabled interactable keeps its marker, with the Disabled state tag, but refuses to start interactions. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteracting() const { return CurrentInteractor != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetElapsedTime() const;

	// Awareness
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnEnteredPlayerRange();
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnLeftPlayerRange();

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnGainedPlayerFocus();
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void OnLostPlayerFocus();

	/** Occluded while any occlusion component is between ViewLocation and the marker. Occluded interactables hide their marker and cannot be focused. */
	void UpdateOcclusion(const FVector& ViewLocation);
	bool IsOccluded() const;

	/** Where the marker is shown. */
	FVector GetInteractionLocation() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	USceneComponent* GetMarkerTarget() const { return MarkerTarget; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UPrimitiveComponent* GetFocusTarget() const { return FocusTarget; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool HasStateTag(UPARAM(meta = (Categories = "Interactable.State")) FGameplayTag Tag) const { return StateTags.HasTagExact(Tag); }

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractableStateTagChangedSignature OnStateTagChanged;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractionProgressedSignature OnInteractionProgressed;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractionSignature OnInteractionStarted;

	/** The interaction was held for the full duration, or started with no duration. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractionSignature OnInteractionCompleted;

	/** The interaction stopped before the duration elapsed. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractionSignature OnInteractionCancelled;

	/** Fires after either Completed or Cancelled. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractionSignature OnInteractionEnded;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/** Runs every frame while an interaction is held. Completes it once the duration has elapsed. */
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void InteractionProgress();

	/** Seconds the interaction must be held. 0 completes on start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "s"))
	float InteractionDuration = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	bool bInteractionEnabled = true;

	/** The collision the player looks at to focus this. Defaults to the actor's root. */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	FComponentPicker FocusComponent;

	/** The component the marker attaches to. Defaults to the actor's root. */
	UPROPERTY(EditAnywhere, Category = "Interaction|Marker")
	FComponentPicker MarkerComponent;

	UPROPERTY(EditAnywhere, Category = "Interaction|Marker")
	TSubclassOf<UHadesWorldMarkerWidget> MarkerWidgetClass;

	/** Components that hide this interactable when they are between the player and the marker. */
	UPROPERTY(EditAnywhere, Category = "Interaction|Occlusion")
	TArray<FComponentPicker> OcclusionComponents;

private:
	void CompleteInteraction();
	void CancelInteraction();
	/** Clears the in-progress state and returns the interactor it belonged to. */
	AActor* ResetInteraction();

	void SetStateTag(const FGameplayTag& Tag, bool bEnabled);
	void UpdateMarkerVisibility();
	UHadesWorldMarkerWidget* GetMarkerWidget() const;

	/** Set while an interaction is in progress. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentInteractor;

	float InteractionStartTime = 0.0f;

	FGameplayTagContainer StateTags;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> FocusTarget;
	void ResolveFocusComponent();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> OcclusionTargets;
	void ResolveOcclusionComponents();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> MarkerTarget;
	void ResolveMarkerComponent();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> MarkerWidgetComponent;
	void InitializeMarkerWidget();
};
