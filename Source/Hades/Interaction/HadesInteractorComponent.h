#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "HadesInteractorComponent.generated.h"

class UHadesInteractableComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesFocusedInteractableChangedSignature, UHadesInteractableComponent*, FocusedInteractable);


/** Lets a player-controlled pawn notice nearby interactables and focus the one it is looking at. */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHadesInteractorComponent();

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	/** Interacts with the focused interactable until StopInteraction is called or focus moves away from it. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void StartInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void StopInteraction();

	/** Turns all interaction off, clearing every marker and the focus, or back on. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UHadesInteractableComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	/** Also fires, with null, when the focused interactable is destroyed. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesFocusedInteractableChangedSignature OnFocusedInteractableChanged;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float NearbyRadius = 750.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float FocusDistance = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.01", Units = "s"))
	float NearbyUpdateInterval = 0.1f;

private:
	void StartNearbyUpdateTimer();
	void UpdateNearbyInteractables();
	UHadesInteractableComponent* TraceForInteractable() const;
	void SetFocusedInteractable(UHadesInteractableComponent* NewFocused);
	void ClearInteractables();

	bool bInteractionEnabled = true;

	TSet<TWeakObjectPtr<UHadesInteractableComponent>> NearbyInteractables;
	TWeakObjectPtr<UHadesInteractableComponent> FocusedInteractable;

	FTimerHandle NearbyUpdateTimerHandle;
};
