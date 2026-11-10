#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "HadesMessageWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;


/** Shows the messages the player controller sends, such as "It's locked." Put one in the root layout, above the layer stacks, so it shows over the HUD and every menu. */
UCLASS(Abstract)
class HADES_API UHadesMessageWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(BlueprintReadOnly, Category = "Message", meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	/** Played from the start for every message, such as fading the text in, holding it and fading it out. Widget animations run while paused. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Message", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

private:
	UFUNCTION()
	void HandleMessage(const FText& Message);
};
