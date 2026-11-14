#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "InventoryUITypes.h"

#include "InventoryStatWidget.generated.h"

class UProgressBar;
class UTextBlock;


/** One of an item's stats in its details: the label, the value and, when comparing, how much better or worse it is than another item's. */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryStatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Max 0 is none. With bCompare, the row also shows how Value differs from Compared Value. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Stat")
	void SetStat(const FInventoryStatDisplay& Display, float Value, float Max, bool bCompare, float ComparedValue);

protected:
	/** The row was filled. Update what the base doesn't, like coloring a low durability. Difference is 0 when not comparing. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Stat")
	void OnStatChanged(const FInventoryStatDisplay& Display, float Value, float Max, bool bCompare, float Difference);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Stat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	/** The value, or the value out of its maximum in Max Format. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Stat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	/** How much better or worse the item is than the one compared, such as +4, in Better Color or Worse Color. Collapsed when they're the same. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Stat", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DifferenceText;

	/** Filled to the value out of its maximum. Collapsed for stats without one. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Stat", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Bar;

	/** Value Text for stats with a maximum. {Value} and {Max} are the numbers. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Stat")
	FText MaxFormat = NSLOCTEXT("InventoryStatWidget", "MaxFormat", "{Value} / {Max}");

	UPROPERTY(EditAnywhere, Category = "Inventory|Stat")
	FLinearColor BetterColor = FLinearColor(FColor(140, 193, 82));

	UPROPERTY(EditAnywhere, Category = "Inventory|Stat")
	FLinearColor WorseColor = FLinearColor(FColor(217, 83, 79));
};
