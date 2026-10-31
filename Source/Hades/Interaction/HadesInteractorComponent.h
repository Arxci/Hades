#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "HadesInteractorComponent.generated.h"

class UHadesInteractableComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHadesInteractionFocusClearedSignature);


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

	/** Turns all interaction off, clearing every marker and the focus, or back on. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UHadesInteractableComponent* GetFocusedInteractable() const { return FocusedInteractable.Get(); }

	/** The focused interactable was destroyed. Listeners should clear any focus-dependent state. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FHadesInteractionFocusClearedSignature OnInteractionFocusCleared;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float AwarenessRadius = 750.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float FocusDistance = 150.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.01", Units = "s"))
	float AwarenessInterval = 0.1f;

private:
	void StartAwarenessTimer();
	void UpdateNearbyInteractables();
	UHadesInteractableComponent* TraceForInteractable() const;
	void SetFocusedInteractable(UHadesInteractableComponent* NewFocused);
	void ClearInteractables();

	bool bInteractionEnabled = true;

	TSet<TWeakObjectPtr<UHadesInteractableComponent>> NearbyInteractables;
	TWeakObjectPtr<UHadesInteractableComponent> FocusedInteractable;

	FTimerHandle AwarenessTimerHandle;
};
