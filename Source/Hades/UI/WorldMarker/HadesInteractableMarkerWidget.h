#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/WorldMarker/HadesWorldMarkerWidget.h"

#include "HadesInteractableMarkerWidget.generated.h"

class UImage;
class UTextBlock;
class UWidget;
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
	//~ Begin UHadesWorldMarkerWidget Interface
	virtual void NativeOnContextChanged() override;
	virtual void NativeOnStateTagChanged(const FGameplayTag& Tag, bool bAdded) override;
	virtual void NativeOnProgressChanged(float NewProgress) override;
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

private:
	void SetFocused(bool bFocused);
	void RefreshPrompt();
	void SetRingProgress(float NewProgress);
};
