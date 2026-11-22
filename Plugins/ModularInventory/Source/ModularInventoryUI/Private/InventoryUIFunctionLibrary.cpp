#include "InventoryUIFunctionLibrary.h"

#include "InventorySubsystem.h"

bool UInventoryUIFunctionLibrary::ResolveValueDisplay(const UObject* WorldContextObject, FInventoryItemHandle Item, const FInventoryValueDisplay& Display, float& OutValue, float& OutMax, float& OutFraction)
{
	OutValue = 0.0f;
	OutMax = 0.0f;
	OutFraction = 0.0f;
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(WorldContextObject);
	if (!Inventory || !Display.Resolve(*Inventory, Item, Inventory->GetItemCount(Item), OutValue, OutMax))
	{
		return false;
	}

	OutFraction = OutMax > 0.0f ? FMath::Clamp(OutValue / OutMax, 0.0f, 1.0f) : 0.0f;
	return true;
}

FText UInventoryUIFunctionLibrary::FormatValueDisplay(const FInventoryValueDisplay& Display, float Value, float Max)
{
	return Display.FormatText(Value, Max);
}

FLinearColor UInventoryUIFunctionLibrary::PickMeterColor(const FInventoryMeterColors& Colors, float Fraction)
{
	return Colors.Pick(Fraction);
}
