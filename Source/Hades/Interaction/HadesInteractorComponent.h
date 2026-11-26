#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "HadesInteractorComponent.generated.h"

class APlayerController;
class UHadesInteractableComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesFocusedInteractableChangedSignature, UHadesInteractableComponent*, FocusedInteractable);


/**
 * Lets a player-controlled pawn notice nearby interactables and focus the one it is looking at. While one with Show Prompt Message is focused,
 * its prompt shows as a message on Prompt Channel, with the key that interacts, such as a banner naming the item the player can pick up, until
 * focus moves on. The message stays until then: it's sent with a Duration of 0.
 */
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

	/** After the look trace leaves the focused interactable without finding another, it stays focused while it's within this angle of the view and Focus Distance of the camera. 0 drops focus as soon as the trace leaves it. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float FocusKeepAngle = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.01", Units = "s"))
	float NearbyUpdateInterval = 0.1f;

	/**
	 * Where the focused interactable's prompt shows, for interactables with Show Prompt Message, a Modular UI message channel such as
	 * UI.Message.Prompt. None shows no prompt.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Prompt", meta = (Categories = "UI.Message"))
	FGameplayTag PromptChannel;

private:
	void StartNearbyUpdateTimer();
	void UpdateNearbyInteractables();
	UHadesInteractableComponent* TraceForInteractable() const;
	bool CanKeepFocus() const;
	float GetFocusDistance() const;
	void SetFocusedInteractable(UHadesInteractableComponent* NewFocused);
	void ClearInteractables();

	/** Shows the interactable's prompt on Prompt Channel, or shows it again after it changed. */
	void ShowPrompt(UHadesInteractableComponent& Interactable);

	/** Hides the prompt shown last, even for an interactable that's gone, such as a pickup just taken. */
	void HidePrompt();
	void HandleFocusedPromptChanged();

	bool bInteractionEnabled = true;

	TSet<TWeakObjectPtr<UHadesInteractableComponent>> NearbyInteractables;
	TWeakObjectPtr<UHadesInteractableComponent> FocusedInteractable;

	/** The key of the prompt shown, and the player it's shown to, to hide it, even once the pawn is unpossessed. */
	FName ShownPromptKey;
	TWeakObjectPtr<APlayerController> PromptViewer;

	FTimerHandle NearbyUpdateTimerHandle;
};
