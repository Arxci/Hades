#include "Data/InventoryContainerLayout.h"

int32 UInventoryContainerLayout::FindSlot(const FGameplayTag& SlotTag) const
{
	if (Type != EInventoryContainerType::Slots)
	{
		return INDEX_NONE;
	}
	return Slots.IndexOfByPredicate([&SlotTag](const FInventorySlotDefinition& Slot) { return Slot.Tag == SlotTag; });
}

bool UInventoryContainerLayout::IsCellUsable(const FIntPoint& Cell) const
{
	const bool bInside = Type == EInventoryContainerType::Grid && Cell.X >= 0 && Cell.Y >= 0 && Cell.X < GridSize.X && Cell.Y < GridSize.Y;
	return bInside && (UsableCells <= 0 || Cell.Y * GridSize.X + Cell.X < UsableCells);
}

int32 UInventoryContainerLayout::GetUsableCellCount() const
{
	const int32 CellCount = GetCellCount();
	return UsableCells > 0 ? FMath::Min(UsableCells, CellCount) : CellCount;
}

int32 UInventoryContainerLayout::GetUsableSlotCount() const
{
	const int32 SlotCount = GetSlotCount();
	return UsableSlots > 0 ? FMath::Min(UsableSlots, SlotCount) : SlotCount;
}

int32 UInventoryContainerLayout::GetStartingCapacity() const
{
	switch (Type)
	{
	case EInventoryContainerType::Grid:
		return GetUsableCellCount();
	case EInventoryContainerType::Slots:
		return GetUsableSlotCount();
	case EInventoryContainerType::List:
		return Capacity > 0 ? Capacity : INDEX_NONE;
	}
	return 0;
}

#if WITH_EDITOR
#include "InventoryValidation.h"

#define LOCTEXT_NAMESPACE "InventoryContainerLayout"

EDataValidationResult UInventoryContainerLayout::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Type == EInventoryContainerType::Grid && (GridSize.X < 1 || GridSize.Y < 1))
	{
		Context.AddError(LOCTEXT("InvalidGridSize", "Grid Size must be at least 1x1."));
		Result = EDataValidationResult::Invalid;
	}

	// More usable cells than the grid has does no harm, but probably means the grid was meant to be bigger.
	if (Type == EInventoryContainerType::Grid && UsableCells > GetCellCount())
	{
		Context.AddWarning(FText::Format(LOCTEXT("TooManyUsableCells", "Usable Cells is {0}, but a {1} x {2} grid only has {3} cells, so none are locked."),
			FText::AsNumber(UsableCells), FText::AsNumber(GridSize.X), FText::AsNumber(GridSize.Y), FText::AsNumber(GetCellCount())));
	}

	if (Type == EInventoryContainerType::Slots)
	{
		if (Slots.IsEmpty())
		{
			Context.AddError(LOCTEXT("NoSlots", "A slot layout needs at least one slot."));
			Result = EDataValidationResult::Invalid;
		}

		// As with Usable Cells: harmless, but probably means a slot is missing.
		if (UsableSlots > Slots.Num())
		{
			Context.AddWarning(FText::Format(LOCTEXT("TooManyUsableSlots", "Usable Slots is {0}, but the layout only has {1} slots, so none are locked."),
				FText::AsNumber(UsableSlots), FText::AsNumber(Slots.Num())));
		}

		// Slots are found by tag, so each tag may appear once.
		TSet<FGameplayTag> SeenTags;
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const FGameplayTag& SlotTag = Slots[Index].Tag;
			bool bAlreadySeen = false;
			SeenTags.Add(SlotTag, &bAlreadySeen);

			if (!SlotTag.IsValid())
			{
				Context.AddError(FText::Format(LOCTEXT("MissingSlotTag", "Slot {0} has no tag."), FText::AsNumber(Index)));
				Result = EDataValidationResult::Invalid;
			}
			else if (bAlreadySeen)
			{
				Context.AddError(FText::Format(LOCTEXT("DuplicateSlotTag", "Slot {0} reuses tag {1}."), FText::AsNumber(Index), FText::FromName(SlotTag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	Result = CombineDataValidationResults(Result, InventoryValidation::ValidateFragments(Fragments, LOCTEXT("Fragments", "Fragments"), Context));
	Result = CombineDataValidationResults(Result, InventoryValidation::ValidateFragments(InstanceFragments, LOCTEXT("InstanceFragments", "Instance Fragments"), Context));

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
