#pragma once

#include "CoreMinimal.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryUIFunctionLibrary.generated.h"


/** The item widget's ways of showing numbers, for any other widget: a HUD's ammo counter, a quick slot, a crafting list. */
UCLASS()
class MODULARINVENTORYUI_API UInventoryUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Works out a Value Display for an item, as an item widget would: whether it applies to the item and isn't hidden, its number, what's
	 * full, Max 0 for no maximum, and how full it is, from 0 to 1, or 0 without a maximum. Custom sources need an item widget, so they're
	 * always false here.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI", meta = (WorldContext = "WorldContextObject"))
	static bool ResolveValueDisplay(const UObject* WorldContextObject, FInventoryItemHandle Item, const FInventoryValueDisplay& Display, float& OutValue, float& OutMax, float& OutFraction);

	/** What the Value Display's Text Block would say for a number: its Format with {Value}, {Max} and {Percent} filled in. */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static FText FormatValueDisplay(const FInventoryValueDisplay& Display, float Value, float Max);

	/** The color for how full something is, from 0 to 1, as the colors say, such as for a bar or a number of your own. */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	static FLinearColor PickMeterColor(const FInventoryMeterColors& Colors, float Fraction);
};
