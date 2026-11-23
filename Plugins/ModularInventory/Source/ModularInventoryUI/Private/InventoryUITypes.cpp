#include "InventoryUITypes.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"

FLinearColor FInventoryMeterColors::Pick(float Fraction) const
{
	if (Fraction <= LowFraction)
	{
		return Low;
	}
	if (Fraction >= FullFraction)
	{
		return Full;
	}
	if (!bBlend)
	{
		return Partly;
	}

	// Low to Partly over the first half of the way from Low Fraction to Full Fraction, then Partly to Full over the second.
	const float Alpha = FMath::Clamp((Fraction - LowFraction) / FMath::Max(FullFraction - LowFraction, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	return Alpha < 0.5f ? FMath::Lerp(Low, Partly, Alpha * 2.0f) : FMath::Lerp(Partly, Full, (Alpha - 0.5f) * 2.0f);
}

bool FInventoryValueDisplay::Resolve(const UInventorySubsystem& Inventory, FInventoryItemHandle Item, int32 Count, float& OutValue, float& OutMax) const
{
	OutValue = 0.0f;
	OutMax = 0.0f;
	const UInventoryItemDefinition* Definition = Inventory.GetItemDefinition(Item);
	if (!Definition || !Inventory.MatchesQuery(Item, ItemFilter))
	{
		return false;
	}

	switch (Source)
	{
	case EInventoryValueSource::StackCount:
	{
		// As high as it stacks where it is, its container's stack bonuses counted.
		const int32 MaxStackSize = Inventory.GetMaxStackSize(Item);
		if (MaxStackSize <= 1 || (bHideSingle && Count <= 1))
		{
			return false;
		}
		OutValue = static_cast<float>(Count);
		OutMax = static_cast<float>(MaxStackSize);
		break;
	}
	case EInventoryValueSource::TagValue:
		if (!Tag.IsValid() || !Inventory.HasTagValue(Item, Tag))
		{
			return false;
		}
		OutValue = bIncludeBonuses ? Inventory.GetEffectiveTagValue(Item, Tag) : Inventory.GetTagValue(Item, Tag);
		OutMax = Inventory.GetTagValueMax(Item, Tag, MaxTag);
		break;
	case EInventoryValueSource::Custom:
		return false;
	}

	return !IsHiddenAt(OutValue, OutMax);
}

FText FInventoryValueDisplay::FormatText(float Value, float Max) const
{
	// Halves round away from zero, as whole numbers always have here.
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(FMath::Clamp(FractionalDigits, 0, 3)).SetRoundingMode(ERoundingMode::HalfFromZero);
	const float Fraction = Max > 0.0f ? FMath::Clamp(Value / Max, 0.0f, 1.0f) : 0.0f;
	return FText::FormatNamed(Format,
		TEXT("Value"), FText::AsNumber(Value, &NumberFormat),
		TEXT("Max"), FText::AsNumber(Max, &NumberFormat),
		TEXT("Percent"), FText::AsNumber(FMath::RoundToInt(Fraction * 100.0f)));
}

bool FInventoryItemPreview::FindTagValue(const FGameplayTag& Tag, float& OutValue) const
{
	const FInventoryTagValue* Found = TagValues.FindByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; });
	if (!Found && Definition)
	{
		Found = Definition->FindTagValue(Tag);
	}
	OutValue = Found ? Found->Value : 0.0f;
	return Found != nullptr;
}

void FInventoryRegistryListener::Bind(const UObject* WorldContextObject, FOnInventoryRegistryChanged::FDelegate&& Delegate)
{
	Unbind();
	if (UInventorySubsystem* Found = UInventorySubsystem::Get(WorldContextObject))
	{
		Subsystem = Found;
		Handle = Found->GetRegistry().OnChanged.Add(MoveTemp(Delegate));
	}
}

void FInventoryRegistryListener::Unbind()
{
	if (UInventorySubsystem* Found = Subsystem.Get())
	{
		Found->GetRegistry().OnChanged.Remove(Handle);
	}
	Subsystem.Reset();
	Handle.Reset();
}
