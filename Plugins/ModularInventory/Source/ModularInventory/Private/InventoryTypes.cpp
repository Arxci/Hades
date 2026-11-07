#include "InventoryTypes.h"

FInventoryPlacement FInventoryPlacement::AtCell(FIntPoint InCell, bool bInRotated)
{
	FInventoryPlacement Placement;
	Placement.Cell = InCell;
	Placement.bRotated = bInRotated;
	return Placement;
}

FInventoryPlacement FInventoryPlacement::AtIndex(int32 InIndex)
{
	FInventoryPlacement Placement;
	Placement.Index = InIndex;
	return Placement;
}

bool FInventoryTagValueCondition::Test(float InValue) const
{
	switch (Comparison)
	{
	case EInventoryComparison::Equal:          return FMath::IsNearlyEqual(InValue, Value);
	case EInventoryComparison::NotEqual:       return !FMath::IsNearlyEqual(InValue, Value);
	case EInventoryComparison::Less:           return InValue < Value;
	case EInventoryComparison::LessOrEqual:    return InValue <= Value;
	case EInventoryComparison::Greater:        return InValue > Value;
	case EInventoryComparison::GreaterOrEqual: return InValue >= Value;
	}
	return false;
}

FInventoryOperation FInventoryOperation::MakeAdd(const FInventoryItemSpec& InSpec, FInventoryContainerHandle InContainer)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Add;
	Result.Spec = InSpec;
	Result.Container = InContainer;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeRemove(FInventoryItemHandle InItem, int32 InCount)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Remove;
	Result.Item = InItem;
	Result.Count = InCount;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeMove(FInventoryItemHandle InItem, FInventoryContainerHandle InContainer, int32 InCount)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Move;
	Result.Item = InItem;
	Result.Container = InContainer;
	Result.Count = InCount;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeMoveTo(FInventoryItemHandle InItem, FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement, int32 InCount)
{
	FInventoryOperation Result = MakeMove(InItem, InContainer, InCount);
	Result.Placement = InPlacement;
	Result.bAutoPlace = false;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeSwap(FInventoryItemHandle InItem, FInventoryItemHandle InOtherItem)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Swap;
	Result.Item = InItem;
	Result.OtherItem = InOtherItem;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeSplit(FInventoryItemHandle InItem, int32 InCount, FInventoryContainerHandle InContainer)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Split;
	Result.Item = InItem;
	Result.Count = InCount;
	Result.Container = InContainer;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeMerge(FInventoryItemHandle InSource, FInventoryItemHandle InTarget, int32 InCount)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Merge;
	Result.Item = InSource;
	Result.OtherItem = InTarget;
	Result.Count = InCount;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeRotate(FInventoryItemHandle InItem)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Rotate;
	Result.Item = InItem;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeEquip(FInventoryItemHandle InItem, FInventoryContainerHandle InEquipmentContainer, FGameplayTag InSlot)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Equip;
	Result.Item = InItem;
	Result.Container = InEquipmentContainer;
	Result.Slot = InSlot;
	return Result;
}

FInventoryOperation FInventoryOperation::MakeUnequip(FInventoryItemHandle InItem, FInventoryContainerHandle InContainer)
{
	FInventoryOperation Result;
	Result.Operation = EInventoryOperation::Unequip;
	Result.Item = InItem;
	Result.Container = InContainer;
	return Result;
}
