#include "Data/InventoryContainerLayout.h"

int32 UInventoryContainerLayout::FindSlot(const FGameplayTag& SlotTag) const
{
	if (Type != EInventoryContainerType::Slots)
	{
		return INDEX_NONE;
	}
	return Slots.IndexOfByPredicate([&SlotTag](const FInventorySlotDefinition& Slot) { return Slot.Tag == SlotTag; });
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

	if (Type == EInventoryContainerType::Slots)
	{
		if (Slots.IsEmpty())
		{
			Context.AddError(LOCTEXT("NoSlots", "A slot layout needs at least one slot."));
			Result = EDataValidationResult::Invalid;
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
