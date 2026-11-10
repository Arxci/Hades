#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "HadesEquippedItemWidget.generated.h"

class UHadesQuickSlotComponent;
class UInventoryItemWidget;
class UTextBlock;


/** The item in the player's hand, such as at the bottom of the inventory screen. */
UCLASS(Abstract)
class HADES_API UHadesEquippedItemWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** Hidden while nothing is in hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryItemWidget> ItemWidget;

	/** The item's name, or Empty Text. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Shown while nothing is in hand, such as a line across the empty box. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipped Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyMarker;

	UPROPERTY(EditAnywhere, Category = "Equipped Item")
	FText EmptyText;

private:
	UFUNCTION()
	void Refresh();

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
};
