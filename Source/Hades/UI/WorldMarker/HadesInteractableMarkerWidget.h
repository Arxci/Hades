#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/WorldMarker/HadesWorldMarkerWidget.h"

#include "HadesInteractableMarkerWidget.generated.h"

class UImage;
class UTextBlock;
class UWidget;
class UWidgetAnimation;
class UHadesInteractableComponent;

/** The marker over an interactable. Shows the indicator while the player is nearby and the prompt while they are focused on it. */
UCLASS(Abstract)
class HADES_API UHadesInteractableMarkerWidget : public UHadesWorldMarkerWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Interactable Marker")
	UHadesInteractableComponent* GetInteractable() const;

protected:
	//~ Begin UUserWidget Interface
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	//~ End UUserWidget Interface

	//~ Begin UHadesWorldMarkerWidget Interface
	virtual void NativeOnContextChanged() override;
	virtual void NativeOnStateTagChanged(const FGameplayTag& Tag, bool bAdded) override;
	virtual void NativeOnProgressChanged(float NewProgress) override;
	virtual void NativeOnEventReceived(const FGameplayTag& EventTag) override;
	virtual bool IsTransitionPlaying() const override;
	//~ End UHadesWorldMarkerWidget Interface

	UPROPERTY(BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidget))
	TObjectPtr<UWidget> Indicator;

	UPROPERTY(BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidget))
	TObjectPtr<UWidget> Prompt;

	/** Shown for hold interactions. Its material needs a scalar parameter named Progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ProgressRing;

	/** Collapsed when the interactable has no text. */
	UPROPERTY(BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionText;

	/** Played forward on focus and in reverse when focus is lost, in place of swapping Indicator and Prompt instantly. Both are visible while it plays. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FocusAnimation;

	/** Loops while focused. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FocusLoopAnimation;

	/** Played when an interaction completes. The progress ring stays full while it plays, and whatever it changes is restored when it ends. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Interactable Marker", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> CompleteAnimation;

	/** While focused, clamps to the screen edge whatever the Edge Mode, so the prompt stays on screen while focus is kept. */
	UPROPERTY(EditDefaultsOnly, Category = "Interactable Marker")
	bool bClampWhileFocused = false;

private:
	void SetFocused(bool bFocused);
	void ShowIndicatorOrPrompt();
	void RefreshPrompt();
	void SetRingProgress(float NewProgress);

	bool bPromptShown = false;
	bool bRingHeldFull = false;
};
