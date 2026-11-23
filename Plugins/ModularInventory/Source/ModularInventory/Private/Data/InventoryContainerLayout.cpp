#include "Data/InventoryContainerLayout.h"

void UInventoryContainerLayout::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Layouts saved with Read Only In UI keep their meaning.
	if (bReadOnlyInUI_DEPRECATED)
	{
		PlayerAccess = EInventoryPlayerAccess::ReadOnly;
		bReadOnlyInUI_DEPRECATED = false;
	}
#endif
}

int32 UInventoryContainerLayout::FindSlot(const FGameplayTag& SlotTag) const
{
	if (Type != EInventoryContainerType::Slots)
	{
		return INDEX_NONE;
	}
	return Slots.IndexOfByPredicate([&SlotTag](const FInventorySlotDefinition& Slot) { return Slot.Tag == SlotTag; });
}

int32 UInventoryContainerLayout::GetMaxRows() const
{
	if (Type != EInventoryContainerType::Grid)
	{
		return 0;
	}

	// Growing only ever adds rows, so Max Rows below Grid Size's changes nothing.
	const int32 StartingRows = FMath::Max(0, GridSize.Y);
	if (GridGrowth == EInventoryGridGrowth::None)
	{
		return StartingRows;
	}
	return MaxRows > 0 ? FMath::Max(StartingRows, MaxRows) : INDEX_NONE;
}

int32 UInventoryContainerLayout::GetGridRowsAtCapacity(int32 AtCapacity) const
{
	if (Type != EInventoryContainerType::Grid || GridSize.X < 1)
	{
		return 0;
	}

	// Cells unlock row by row, so the capacity reaches down to the row its last cell is on, and every row of Grid Size always counts.
	const int32 StartingRows = FMath::Max(0, GridSize.Y);
	if (GridGrowth != EInventoryGridGrowth::WithUpgrades || AtCapacity == INDEX_NONE)
	{
		return StartingRows;
	}
	const int32 Rows = FMath::Max(StartingRows, FMath::DivideAndRoundUp(FMath::Max(0, AtCapacity), GridSize.X));
	const int32 Most = GetMaxRows();
	return Most == INDEX_NONE ? Rows : FMath::Min(Rows, Most);
}

int32 UInventoryContainerLayout::GetMaxCapacity() const
{
	switch (Type)
	{
	case EInventoryContainerType::Grid:
	{
		const int32 Most = GetMaxRows();
		if (Most == INDEX_NONE)
		{
			return INDEX_NONE;
		}
		const int64 Cells = int64(FMath::Max(0, GridSize.X)) * Most;
		return static_cast<int32>(FMath::Min<int64>(Cells, MAX_int32));
	}
	case EInventoryContainerType::Slots:
		return GetSlotCount();
	case EInventoryContainerType::List:
		break;
	}
	return INDEX_NONE;
}

int32 UInventoryContainerLayout::GetStartingCapacity() const
{
	switch (Type)
	{
	case EInventoryContainerType::Grid:
		// A grid that grows to fit can take anything up to Max Rows from the start. Others start at Grid Size, and Max Rows is only room
		// for upgrades to grow into.
		return GridGrowth == EInventoryGridGrowth::ToFitItems ? GetMaxCapacity() : GetCellCount();
	case EInventoryContainerType::Slots:
		return UsableSlots > 0 ? ClampCapacity(UsableSlots) : GetSlotCount();
	case EInventoryContainerType::List:
		return Capacity > 0 ? Capacity : INDEX_NONE;
	}
	return 0;
}

int32 UInventoryContainerLayout::ClampCapacity(int32 Count) const
{
	if (Count < 0)
	{
		return INDEX_NONE;
	}
	const int32 Most = GetMaxCapacity();
	return Most == INDEX_NONE ? Count : FMath::Min(Count, Most);
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

	// Max Rows only ever adds rows, so one below Grid Size's rows was probably meant to be higher.
	if (CanGridGrow() && MaxRows > 0 && MaxRows < GridSize.Y)
	{
		Context.AddWarning(FText::Format(LOCTEXT("MaxRowsBelowGridSize", "Max Rows is {0}, fewer than Grid Size's {1} rows, so the grid can't grow. Set it higher, or to 0 for no limit."),
			FText::AsNumber(MaxRows), FText::AsNumber(GridSize.Y)));
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
