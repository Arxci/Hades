#include "InventoryRegistry.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"

// Placement checks
EInventoryError FInventoryRegistry::CanPlace(FInventoryItemHandle Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}

	const EInventoryError Error = CanEnter(Item.Index, *Record, Container);
	return Error != EInventoryError::None ? Error : CanOccupy(Item.Index, *Record, Container, Placement);
}

bool FInventoryRegistry::FindPlacement(FInventoryItemHandle Item, FInventoryContainerHandle Container, FInventoryPlacement& OutPlacement) const
{
	const FInventoryItemRecord* Record = FindItem(Item);
	return Record && CanEnter(Item.Index, *Record, Container) == EInventoryError::None && FindPlacementFor(Item.Index, *Record, Container, OutPlacement);
}

EInventoryError FInventoryRegistry::CanEnter(int32 ItemIndex, const FInventoryItemRecord& Item, FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* ContainerRecord = FindContainer(Container);
	if (!ContainerRecord)
	{
		return EInventoryError::InvalidContainer;
	}
	if (!Item.Definition)
	{
		return EInventoryError::InvalidDefinition;
	}
	if (ItemIndex != INDEX_NONE && IsInsideItem(Container, MakeHandle(ItemIndex)))
	{
		return EInventoryError::WouldContainItself;
	}
	if (!RecordMatchesQuery(Item, ContainerRecord->Layout->Filter))
	{
		return EInventoryError::Rejected;
	}
	return CanEnterHook ? CanEnterHook(*this, Item, Container) : EInventoryError::None;
}

EInventoryError FInventoryRegistry::CanOccupy(int32 ItemIndex, const FInventoryItemRecord& Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const
{
	const FInventoryContainerRecord* ContainerRecord = FindContainer(Container);
	if (!ContainerRecord)
	{
		return EInventoryError::InvalidContainer;
	}

	const UInventoryContainerLayout& Layout = *ContainerRecord->Layout;
	switch (Layout.Type)
	{
	case EInventoryContainerType::Grid:
	{
		if (Placement.bRotated && !Item.Definition->bCanRotate)
		{
			return EInventoryError::CannotRotate;
		}

		const FIntPoint Size = Item.Definition->GetSize(Placement.bRotated);
		const FIntPoint& Cell = Placement.Cell;
		const bool bLayoutChanged = ContainerRecord->Cells.Num() != Layout.GetCellCount();
		if (bLayoutChanged || Cell.X < 0 || Cell.Y < 0 || Cell.X + Size.X > Layout.GridSize.X || Cell.Y + Size.Y > Layout.GridSize.Y)
		{
			return EInventoryError::OutOfBounds;
		}

		for (int32 Y = Cell.Y; Y < Cell.Y + Size.Y; ++Y)
		{
			for (int32 X = Cell.X; X < Cell.X + Size.X; ++X)
			{
				const int32 Occupant = ContainerRecord->Cells[Y * Layout.GridSize.X + X];
				if (Occupant != INDEX_NONE && Occupant != ItemIndex)
				{
					return EInventoryError::Occupied;
				}
			}
		}
		return EInventoryError::None;
	}
	case EInventoryContainerType::Slots:
	{
		if (!Layout.Slots.IsValidIndex(Placement.Index) || !ContainerRecord->Cells.IsValidIndex(Placement.Index))
		{
			return EInventoryError::OutOfBounds;
		}

		const int32 Occupant = ContainerRecord->Cells[Placement.Index];
		if (Occupant != INDEX_NONE && Occupant != ItemIndex)
		{
			return EInventoryError::Occupied;
		}
		if (!RecordMatchesQuery(Item, Layout.Slots[Placement.Index].Filter))
		{
			return EInventoryError::Rejected;
		}
		return Layout.bEquipment ? CanEquipInSlot(ItemIndex, Item, *ContainerRecord, Placement.Index) : EInventoryError::None;
	}
	case EInventoryContainerType::List:
	{
		const bool bAlreadyInside = Item.Container == Container;
		const bool bFull = Layout.Capacity > 0 && ContainerRecord->Items.Num() >= Layout.Capacity;
		return bFull && !bAlreadyInside ? EInventoryError::NoSpace : EInventoryError::None;
	}
	}
	return EInventoryError::None;
}

EInventoryError FInventoryRegistry::CanEquipInSlot(int32 ItemIndex, const FInventoryItemRecord& Item, const FInventoryContainerRecord& Container, int32 SlotIndex) const
{
	const UInventoryEquipRules* Rules = Item.Definition->EquipRules;
	if (!Rules || !Rules->AllowsSlot(Container.Layout->Slots[SlotIndex].Tag))
	{
		return EInventoryError::NotEquippable;
	}

	// The slot itself is already known to be free, so any conflict is a blocked slot.
	return FindEquipConflicts(ItemIndex, Item, Container, SlotIndex).IsEmpty() ? EInventoryError::None : EInventoryError::NotEquippable;
}

TArray<int32> FInventoryRegistry::FindEquipConflicts(int32 ItemIndex, const FInventoryItemRecord& Item, const FInventoryContainerRecord& Container, int32 SlotIndex) const
{
	const UInventoryEquipRules* Rules = Item.Definition->EquipRules;
	const TArray<FInventorySlotDefinition>& Slots = Container.Layout->Slots;
	const FGameplayTag& SlotTag = Slots[SlotIndex].Tag;

	// Blocking works both ways: a two-handed weapon needs the off hand free, and a full off hand keeps it out.
	TArray<int32> Conflicts;
	for (int32 OtherSlot = 0; OtherSlot < Container.Cells.Num() && OtherSlot < Slots.Num(); ++OtherSlot)
	{
		const int32 OtherItem = Container.Cells[OtherSlot];
		if (OtherItem == INDEX_NONE || OtherItem == ItemIndex)
		{
			continue;
		}

		const UInventoryEquipRules* OtherRules = Items[OtherItem].Definition->EquipRules;
		const bool bInTheWay = OtherSlot == SlotIndex
			|| (Rules && Rules->BlocksSlot(Slots[OtherSlot].Tag))
			|| (OtherRules && OtherRules->BlocksSlot(SlotTag));
		if (bInTheWay)
		{
			Conflicts.Add(OtherItem);
		}
	}
	return Conflicts;
}

bool FInventoryRegistry::FindPlacementFor(int32 ItemIndex, const FInventoryItemRecord& Item, FInventoryContainerHandle Container, FInventoryPlacement& OutPlacement) const
{
	const FInventoryContainerRecord* ContainerRecord = FindContainer(Container);
	if (!ContainerRecord)
	{
		return false;
	}

	const UInventoryContainerLayout& Layout = *ContainerRecord->Layout;
	switch (Layout.Type)
	{
	case EInventoryContainerType::Grid:
	{
		const bool bPreferRotated = Item.Placement.bRotated;
		for (const bool bRotated : { bPreferRotated, !bPreferRotated })
		{
			if (bRotated && !Item.Definition->bCanRotate)
			{
				continue;
			}

			const FIntPoint Size = Item.Definition->GetSize(bRotated);
			for (int32 Y = 0; Y + Size.Y <= Layout.GridSize.Y; ++Y)
			{
				for (int32 X = 0; X + Size.X <= Layout.GridSize.X; ++X)
				{
					const FInventoryPlacement Candidate = FInventoryPlacement::AtCell(FIntPoint(X, Y), bRotated);
					if (CanOccupy(ItemIndex, Item, Container, Candidate) == EInventoryError::None)
					{
						OutPlacement = Candidate;
						return true;
					}
				}
			}
		}
		return false;
	}
	case EInventoryContainerType::Slots:
		for (int32 SlotIndex = 0; SlotIndex < Layout.Slots.Num(); ++SlotIndex)
		{
			const FInventoryPlacement Candidate = FInventoryPlacement::AtIndex(SlotIndex);
			if (CanOccupy(ItemIndex, Item, Container, Candidate) == EInventoryError::None)
			{
				OutPlacement = Candidate;
				return true;
			}
		}
		return false;
	case EInventoryContainerType::List:
		OutPlacement = FInventoryPlacement::AtIndex(ContainerRecord->Items.Num());
		return CanOccupy(ItemIndex, Item, Container, OutPlacement) == EInventoryError::None;
	}
	return false;
}

// Operations
EInventoryError FInventoryRegistry::AddItem(const FInventoryItemSpec& Spec, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement, TArray<FInventoryItemHandle>* OutItems)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_AddItem);

	if (!Placement.IsSet())
	{
		EInventoryError Error = EInventoryError::None;
		AddItemToContainers(Spec, MakeArrayView(&Container, 1), false, OutItems, &Error);
		return Error;
	}

	if (!Spec.Definition)
	{
		return EInventoryError::InvalidDefinition;
	}
	if (Spec.Count < 1 || Spec.Count > Spec.Definition->GetMaxStackSize())
	{
		return EInventoryError::InvalidCount;
	}
	if (!IsValid(Container))
	{
		return EInventoryError::InvalidContainer;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		FInventoryItemRecord Record = MakeItemRecord(Spec);
		EInventoryError Error = CanEnter(INDEX_NONE, Record, Container);
		if (Error != EInventoryError::None)
		{
			return Error;
		}

		const int32 Occupant = FindOccupant(Containers[Container.Index], *Placement);
		if (Occupant != INDEX_NONE && RecordsCanStack(Record, Items[Occupant]))
		{
			if (Items[Occupant].Count + Spec.Count > Spec.Definition->GetMaxStackSize())
			{
				return EInventoryError::CannotStack;
			}
			EditItem(Occupant).Count += Spec.Count;
			if (OutItems)
			{
				OutItems->Add(MakeHandle(Occupant));
			}
			return EInventoryError::None;
		}

		Error = CanOccupy(INDEX_NONE, Record, Container, *Placement);
		if (Error != EInventoryError::None)
		{
			return Error;
		}

		const int32 NewIndex = CreateItem(MoveTemp(Record));
		AttachItem(NewIndex, Container, *Placement);
		if (OutItems)
		{
			OutItems->Add(MakeHandle(NewIndex));
		}
		return EInventoryError::None;
	});
}

int32 FInventoryRegistry::AddItemToContainers(const FInventoryItemSpec& Spec, TConstArrayView<FInventoryContainerHandle> TargetContainers, bool bAllowPartial, TArray<FInventoryItemHandle>* OutItems, EInventoryError* OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_AddItemToContainers);

	int32 Added = 0;
	const EInventoryError Error = RunTransaction([&]() -> EInventoryError
	{
		if (!Spec.Definition)
		{
			return EInventoryError::InvalidDefinition;
		}
		if (Spec.Count < 1)
		{
			return EInventoryError::InvalidCount;
		}

		const int32 MaxStack = Spec.Definition->GetMaxStackSize();
		const FInventoryItemRecord Prototype = MakeItemRecord(Spec);
		int32 Remaining = Spec.Count;

		// Top up existing stacks before taking new space.
		for (const FInventoryContainerHandle& Container : TargetContainers)
		{
			if (MaxStack <= 1 || Remaining == 0 || CanEnter(INDEX_NONE, Prototype, Container) != EInventoryError::None)
			{
				continue;
			}

			for (const FInventoryItemHandle& Existing : Containers[Container.Index].Items)
			{
				const FInventoryItemRecord& ExistingRecord = Items[Existing.Index];
				if (Remaining == 0 || ExistingRecord.Count >= MaxStack || !RecordsCanStack(Prototype, ExistingRecord))
				{
					continue;
				}

				const int32 Moved = FMath::Min(Remaining, MaxStack - ExistingRecord.Count);
				EditItem(Existing.Index).Count += Moved;
				Remaining -= Moved;
				if (OutItems)
				{
					OutItems->AddUnique(Existing);
				}
			}
		}

		while (Remaining > 0)
		{
			FInventoryContainerHandle Target;
			FInventoryPlacement Placement;
			for (const FInventoryContainerHandle& Container : TargetContainers)
			{
				if (CanEnter(INDEX_NONE, Prototype, Container) == EInventoryError::None && FindPlacementFor(INDEX_NONE, Prototype, Container, Placement))
				{
					Target = Container;
					break;
				}
			}
			if (!Target.IsSet())
			{
				break;
			}

			FInventoryItemRecord Record = Prototype;
			Record.Count = FMath::Min(Remaining, MaxStack);
			Remaining -= Record.Count;

			const int32 NewIndex = CreateItem(MoveTemp(Record));
			AttachItem(NewIndex, Target, Placement);
			if (OutItems)
			{
				OutItems->Add(MakeHandle(NewIndex));
			}
		}

		Added = Spec.Count - Remaining;
		const bool bAcceptable = Remaining == 0 || (bAllowPartial && Added > 0);
		return bAcceptable ? EInventoryError::None : EInventoryError::NoSpace;
	});

	if (OutError)
	{
		*OutError = Error;
	}
	return Error == EInventoryError::None ? Added : 0;
}

int32 FInventoryRegistry::RemoveItem(FInventoryItemHandle Item, int32 Count)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_RemoveItem);

	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return 0;
	}

	const int32 Removed = Count <= 0 || Count >= Record->Count ? Record->Count : Count;
	RunTransaction([&]()
	{
		if (Removed == Items[Item.Index].Count)
		{
			DestroyItemRecord(Item.Index);
		}
		else
		{
			EditItem(Item.Index).Count -= Removed;
		}
		return EInventoryError::None;
	});
	return Removed;
}

EInventoryError FInventoryRegistry::MoveItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement, int32 Count)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_MoveItem);

	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}
	if (!IsValid(Container))
	{
		return EInventoryError::InvalidContainer;
	}
	if (Count < 0 || Count > Record->Count)
	{
		return EInventoryError::InvalidCount;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		int32 MovingIndex = Item.Index;
		if (Count > 0 && Count < Items[Item.Index].Count)
		{
			if (!Items[Item.Index].Definition->CanSplit())
			{
				return EInventoryError::CannotStack;
			}
			MovingIndex = SplitOff(Item.Index, Count);
		}
		return PlaceWholeStack(MovingIndex, Container, Placement);
	});
}

EInventoryError FInventoryRegistry::SwapItems(FInventoryItemHandle Item, FInventoryItemHandle Other)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_SwapItems);

	if (!IsValid(Item) || !IsValid(Other) || Item == Other)
	{
		return EInventoryError::InvalidItem;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		const FInventoryContainerHandle ContainerA = Items[Item.Index].Container;
		const FInventoryContainerHandle ContainerB = Items[Other.Index].Container;

		// Each takes the other's spot but keeps its own rotation.
		FInventoryPlacement PlacementForA = GetItemPlacement(Other);
		FInventoryPlacement PlacementForB = GetItemPlacement(Item);
		PlacementForA.bRotated = Items[Item.Index].Placement.bRotated;
		PlacementForB.bRotated = Items[Other.Index].Placement.bRotated;

		DetachItem(Item.Index);
		DetachItem(Other.Index);

		struct FSwapMove
		{
			int32 ItemIndex;
			FInventoryContainerHandle Container;
			FInventoryPlacement Placement;
		};
		const FSwapMove Moves[] = { { Item.Index, ContainerB, PlacementForA }, { Other.Index, ContainerA, PlacementForB } };

		for (const FSwapMove& SwapMove : Moves)
		{
			EInventoryError Error = CanEnter(SwapMove.ItemIndex, Items[SwapMove.ItemIndex], SwapMove.Container);
			if (Error == EInventoryError::None)
			{
				Error = CanOccupy(SwapMove.ItemIndex, Items[SwapMove.ItemIndex], SwapMove.Container, SwapMove.Placement);
			}
			if (Error != EInventoryError::None)
			{
				return Error;
			}
			AttachItem(SwapMove.ItemIndex, SwapMove.Container, SwapMove.Placement);
		}
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::SplitStack(FInventoryItemHandle Item, int32 Count, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement, FInventoryItemHandle* OutNewItem)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_SplitStack);

	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}
	if (!Record->Definition->CanSplit())
	{
		return EInventoryError::CannotStack;
	}
	if (Count < 1 || Count >= Record->Count)
	{
		return EInventoryError::InvalidCount;
	}

	const FInventoryContainerHandle Target = Container.IsSet() ? Container : Record->Container;
	return RunTransaction([&]() -> EInventoryError
	{
		const int32 NewIndex = SplitOff(Item.Index, Count);

		FInventoryPlacement Spot;
		EInventoryError Error = CanEnter(NewIndex, Items[NewIndex], Target);
		if (Error == EInventoryError::None)
		{
			if (Placement.IsSet())
			{
				Spot = *Placement;
				Error = CanOccupy(NewIndex, Items[NewIndex], Target, Spot);
			}
			else if (!FindPlacementFor(NewIndex, Items[NewIndex], Target, Spot))
			{
				Error = EInventoryError::NoSpace;
			}
		}
		if (Error != EInventoryError::None)
		{
			return Error;
		}

		AttachItem(NewIndex, Target, Spot);
		if (OutNewItem)
		{
			*OutNewItem = MakeHandle(NewIndex);
		}
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::MergeStacks(FInventoryItemHandle Source, FInventoryItemHandle Target, int32 Count, int32* OutMoved)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_MergeStacks);

	if (OutMoved)
	{
		*OutMoved = 0;
	}

	const FInventoryItemRecord* SourceRecord = FindItem(Source);
	const FInventoryItemRecord* TargetRecord = FindItem(Target);
	if (!SourceRecord || !TargetRecord || Source == Target)
	{
		return EInventoryError::InvalidItem;
	}
	if (!RecordsCanStack(*SourceRecord, *TargetRecord))
	{
		return EInventoryError::CannotStack;
	}

	const int32 Requested = Count <= 0 ? SourceRecord->Count : FMath::Min(Count, SourceRecord->Count);
	const int32 Moved = FMath::Min(Requested, TargetRecord->Definition->GetMaxStackSize() - TargetRecord->Count);
	if (Moved <= 0)
	{
		return EInventoryError::CannotStack;
	}

	const EInventoryError Error = RunTransaction([&]()
	{
		EditItem(Target.Index).Count += Moved;
		if (Moved == Items[Source.Index].Count)
		{
			DestroyItemRecord(Source.Index);
		}
		else
		{
			EditItem(Source.Index).Count -= Moved;
		}
		return EInventoryError::None;
	});

	if (Error == EInventoryError::None && OutMoved)
	{
		*OutMoved = Moved;
	}
	return Error;
}

EInventoryError FInventoryRegistry::RotateItem(FInventoryItemHandle Item)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_RotateItem);

	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}

	const FInventoryContainerHandle Container = Record->Container;
	const FInventoryContainerRecord* ContainerRecord = FindContainer(Container);
	if (!ContainerRecord || ContainerRecord->Layout->Type != EInventoryContainerType::Grid || !Record->Definition->bCanRotate)
	{
		return EInventoryError::CannotRotate;
	}

	FInventoryPlacement Rotated = Record->Placement;
	Rotated.bRotated = !Rotated.bRotated;
	const EInventoryError Error = CanOccupy(Item.Index, *Record, Container, Rotated);
	if (Error != EInventoryError::None)
	{
		return Error;
	}

	return RunTransaction([&]()
	{
		DetachItem(Item.Index);
		AttachItem(Item.Index, Container, Rotated);
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::EquipItem(FInventoryItemHandle Item, FInventoryContainerHandle EquipmentContainer, const FGameplayTag& Slot, TConstArrayView<FInventoryContainerHandle> DisplaceTo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_EquipItem);

	const FInventoryContainerRecord* ContainerRecord = FindContainer(EquipmentContainer);
	if (!ContainerRecord)
	{
		return EInventoryError::InvalidContainer;
	}
	if (!ContainerRecord->Layout->bEquipment)
	{
		return EInventoryError::NotEquippable;
	}

	const int32 SlotIndex = ContainerRecord->Layout->FindSlot(Slot);
	if (SlotIndex == INDEX_NONE)
	{
		return EInventoryError::OutOfBounds;
	}

	const FInventoryPlacement SlotPlacement = FInventoryPlacement::AtIndex(SlotIndex);
	const FInventoryItemRecord* Record = FindItem(Item);
	const TArray<int32> Conflicts = Record && !DisplaceTo.IsEmpty() ? FindEquipConflicts(Item.Index, *Record, *ContainerRecord, SlotIndex) : TArray<int32>();
	if (Conflicts.IsEmpty())
	{
		return MoveItem(Item, EquipmentContainer, SlotPlacement);
	}

	const FInventoryContainerHandle OldContainer = Record->Container;
	const FInventoryPlacement OldPlacement = GetItemPlacement(Item);
	return RunTransaction([&]() -> EInventoryError
	{
		// Lifted out first, so the item can take the slot and they can use the space it leaves.
		for (const int32 Conflict : Conflicts)
		{
			DetachItem(Conflict);
		}

		EInventoryError Error = PlaceWholeStack(Item.Index, EquipmentContainer, SlotPlacement);
		for (int32 Index = 0; Error == EInventoryError::None && Index < Conflicts.Num(); ++Index)
		{
			Error = PlaceDisplaced(Conflicts[Index], OldContainer, OldPlacement, DisplaceTo);
		}
		return Error;
	});
}

EInventoryError FInventoryRegistry::UnequipItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement)
{
	const FInventoryItemRecord* Record = FindItem(Item);
	const FInventoryContainerRecord* Current = Record ? FindContainer(Record->Container) : nullptr;
	if (!Current || !Current->Layout->bEquipment)
	{
		return EInventoryError::InvalidItem;
	}
	return MoveItem(Item, Container, Placement);
}

// Helpers for the operations
int32 FInventoryRegistry::SplitOff(int32 ItemIndex, int32 Count)
{
	FInventoryItemRecord Record = Items[ItemIndex];
	Record.Count = Count;
	EditItem(ItemIndex).Count -= Count;
	return CreateItem(MoveTemp(Record));
}

EInventoryError FInventoryRegistry::PlaceWholeStack(int32 ItemIndex, FInventoryContainerHandle Container, const TOptional<FInventoryPlacement>& Placement)
{
	const EInventoryError EnterError = CanEnter(ItemIndex, Items[ItemIndex], Container);
	if (EnterError != EInventoryError::None)
	{
		return EnterError;
	}

	const int32 MaxStack = Items[ItemIndex].Definition->GetMaxStackSize();

	if (Placement.IsSet())
	{
		// Dropped onto a stack it can join.
		const int32 Occupant = FindOccupant(Containers[Container.Index], *Placement);
		if (Occupant != INDEX_NONE && Occupant != ItemIndex && RecordsCanStack(Items[ItemIndex], Items[Occupant]))
		{
			if (Items[Occupant].Count + Items[ItemIndex].Count > MaxStack)
			{
				return EInventoryError::CannotStack;
			}
			EditItem(Occupant).Count += Items[ItemIndex].Count;
			DestroyItemRecord(ItemIndex);
			return EInventoryError::None;
		}

		const EInventoryError Error = CanOccupy(ItemIndex, Items[ItemIndex], Container, *Placement);
		if (Error != EInventoryError::None)
		{
			return Error;
		}
		DetachItem(ItemIndex);
		AttachItem(ItemIndex, Container, *Placement);
		return EInventoryError::None;
	}

	if (TopUpStacks(ItemIndex, Container) || Items[ItemIndex].Container == Container)
	{
		return EInventoryError::None;
	}

	FInventoryPlacement FreeSpot;
	if (!FindPlacementFor(ItemIndex, Items[ItemIndex], Container, FreeSpot))
	{
		return EInventoryError::NoSpace;
	}
	DetachItem(ItemIndex);
	AttachItem(ItemIndex, Container, FreeSpot);
	return EInventoryError::None;
}

bool FInventoryRegistry::TopUpStacks(int32 ItemIndex, FInventoryContainerHandle Container)
{
	const int32 MaxStack = Items[ItemIndex].Definition->GetMaxStackSize();
	if (MaxStack <= 1)
	{
		return false;
	}

	// Copied: emptying the moving stack destroys it, which edits this list when it's the same container.
	const TArray<FInventoryItemHandle> Stacks = Containers[Container.Index].Items;
	for (const FInventoryItemHandle& Stack : Stacks)
	{
		if (Stack.Index == ItemIndex || Items[Stack.Index].Count >= MaxStack || !RecordsCanStack(Items[ItemIndex], Items[Stack.Index]))
		{
			continue;
		}

		const int32 Moved = FMath::Min(Items[ItemIndex].Count, MaxStack - Items[Stack.Index].Count);
		EditItem(Stack.Index).Count += Moved;
		if (Moved == Items[ItemIndex].Count)
		{
			DestroyItemRecord(ItemIndex);
			return true;
		}
		EditItem(ItemIndex).Count -= Moved;
	}
	return false;
}

EInventoryError FInventoryRegistry::PlaceDisplaced(int32 ItemIndex, FInventoryContainerHandle SpotContainer, FInventoryPlacement Spot, TConstArrayView<FInventoryContainerHandle> Targets)
{
	Spot.bRotated = Items[ItemIndex].Placement.bRotated;
	if (IsValid(SpotContainer) && PlaceWholeStack(ItemIndex, SpotContainer, Spot) == EInventoryError::None)
	{
		return EInventoryError::None;
	}

	for (const FInventoryContainerHandle& Target : Targets)
	{
		if (IsValid(Target) && PlaceWholeStack(ItemIndex, Target, {}) == EInventoryError::None)
		{
			return EInventoryError::None;
		}
	}
	return EInventoryError::NoSpace;
}

EInventoryError FInventoryRegistry::MoveItemToContainers(FInventoryItemHandle Item, TConstArrayView<FInventoryContainerHandle> TargetContainers, int32 Count, bool bAllowPartial)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_MoveItemToContainers);

	const FInventoryItemRecord* Record = FindItem(Item);
	if (!Record)
	{
		return EInventoryError::InvalidItem;
	}
	if (Count < 0 || Count > Record->Count)
	{
		return EInventoryError::InvalidCount;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		int32 MovingIndex = Item.Index;
		if (Count > 0 && Count < Items[Item.Index].Count)
		{
			if (!Items[Item.Index].Definition->CanSplit())
			{
				return EInventoryError::CannotStack;
			}
			MovingIndex = SplitOff(Item.Index, Count);
		}

		const int32 MovingCount = Items[MovingIndex].Count;
		for (const FInventoryContainerHandle& Container : TargetContainers)
		{
			if (CanEnter(MovingIndex, Items[MovingIndex], Container) == EInventoryError::None && TopUpStacks(MovingIndex, Container))
			{
				return EInventoryError::None;
			}
		}

		if (TargetContainers.Contains(Items[MovingIndex].Container))
		{
			return EInventoryError::None;
		}

		for (const FInventoryContainerHandle& Container : TargetContainers)
		{
			FInventoryPlacement FreeSpot;
			if (CanEnter(MovingIndex, Items[MovingIndex], Container) == EInventoryError::None && FindPlacementFor(MovingIndex, Items[MovingIndex], Container, FreeSpot))
			{
				DetachItem(MovingIndex);
				AttachItem(MovingIndex, Container, FreeSpot);
				return EInventoryError::None;
			}
		}

		const bool bSomethingMoved = Items[MovingIndex].Count < MovingCount;
		if (!bAllowPartial || !bSomethingMoved)
		{
			return TargetContainers.IsEmpty() ? EInventoryError::InvalidContainer : EInventoryError::NoSpace;
		}

		// A split-off remainder has nowhere to be, so it goes back onto the stack it came from.
		if (MovingIndex != Item.Index)
		{
			EditItem(Item.Index).Count += Items[MovingIndex].Count;
			DestroyItemRecord(MovingIndex);
		}
		return EInventoryError::None;
	});
}

// Containers
EInventoryError FInventoryRegistry::SetContainerLayout(FInventoryContainerHandle Container, const UInventoryContainerLayout* Layout)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_SetContainerLayout);

	const FInventoryContainerRecord* Record = FindContainer(Container);
	if (!Record)
	{
		return EInventoryError::InvalidContainer;
	}
	if (!Layout || Layout->Type != Record->Layout->Type)
	{
		return EInventoryError::Rejected;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		FInventoryContainerRecord& Edited = EditContainer(Container.Index);
		Edited.Layout = Layout;
		Edited.Cells.Init(INDEX_NONE, Layout->Type == EInventoryContainerType::Grid ? Layout->GetCellCount() : Layout->GetSlotCount());
		if (Layout->Type == EInventoryContainerType::List && Layout->Capacity > 0 && Edited.Items.Num() > Layout->Capacity)
		{
			return EInventoryError::NoSpace;
		}

		// Stamped one at a time, so each item is checked against those already back in place.
		const TArray<FInventoryItemHandle> Contents = Edited.Items;
		for (const FInventoryItemHandle& Item : Contents)
		{
			EInventoryError Error = CanEnter(Item.Index, Items[Item.Index], Container);
			if (Error == EInventoryError::None)
			{
				Error = CanOccupy(Item.Index, Items[Item.Index], Container, Items[Item.Index].Placement);
			}
			if (Error != EInventoryError::None)
			{
				return Error;
			}
			StampCells(Container.Index, Item.Index, Item.Index);
		}
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::SortContainer(FInventoryContainerHandle Container)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_SortContainer);

	const FInventoryContainerRecord* Record = FindContainer(Container);
	if (!Record)
	{
		return EInventoryError::InvalidContainer;
	}
	if (Record->Layout->Type != EInventoryContainerType::Grid)
	{
		return EInventoryError::Rejected;
	}

	TArray<FInventoryItemHandle> Sorted = Record->Items;
	Sorted.StableSort([this](const FInventoryItemHandle& A, const FInventoryItemHandle& B)
	{
		const FIntPoint SizeA = Items[A.Index].Definition->Size;
		const FIntPoint SizeB = Items[B.Index].Definition->Size;
		return SizeA.X * SizeA.Y > SizeB.X * SizeB.Y;
	});

	return RunTransaction([&]() -> EInventoryError
	{
		for (const FInventoryItemHandle& Item : Sorted)
		{
			DetachItem(Item.Index);
		}

		for (const FInventoryItemHandle& Item : Sorted)
		{
			if (TopUpStacks(Item.Index, Container))
			{
				continue;
			}

			FInventoryPlacement Spot;
			if (!FindPlacementFor(Item.Index, Items[Item.Index], Container, Spot))
			{
				return EInventoryError::NoSpace;
			}
			AttachItem(Item.Index, Container, Spot);
		}
		return EInventoryError::None;
	});
}

// Batches
FInventoryBatchResult FInventoryRegistry::ExecuteBatch(TConstArrayView<FInventoryOperation> Operations)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_ExecuteBatch);

	FInventoryBatchResult Result;
	RunTransaction([&]() -> EInventoryError
	{
		for (int32 Index = 0; Index < Operations.Num(); ++Index)
		{
			const EInventoryError Error = ExecuteOperation(Operations[Index]);
			if (Error != EInventoryError::None)
			{
				Result.Error = Error;
				Result.FailedOperation = Index;
				return Error;
			}
		}
		return EInventoryError::None;
	});
	return Result;
}

EInventoryError FInventoryRegistry::ExecuteOperation(const FInventoryOperation& Operation)
{
	const TOptional<FInventoryPlacement> Placement = Operation.bAutoPlace ? TOptional<FInventoryPlacement>() : TOptional<FInventoryPlacement>(Operation.Placement);

	switch (Operation.Operation)
	{
	case EInventoryOperation::Add:
		return AddItem(Operation.Spec, Operation.Container, Placement);
	case EInventoryOperation::Remove:
		return RemoveItem(Operation.Item, Operation.Count) > 0 ? EInventoryError::None : EInventoryError::InvalidItem;
	case EInventoryOperation::Move:
		return MoveItem(Operation.Item, Operation.Container, Placement, Operation.Count);
	case EInventoryOperation::Swap:
		return SwapItems(Operation.Item, Operation.OtherItem);
	case EInventoryOperation::Split:
		return SplitStack(Operation.Item, Operation.Count, Operation.Container, Placement);
	case EInventoryOperation::Merge:
		return MergeStacks(Operation.Item, Operation.OtherItem, Operation.Count);
	case EInventoryOperation::Rotate:
		return RotateItem(Operation.Item);
	case EInventoryOperation::Equip:
		return EquipItem(Operation.Item, Operation.Container, Operation.Slot);
	case EInventoryOperation::Unequip:
		return UnequipItem(Operation.Item, Operation.Container, Placement);
	}
	return EInventoryError::InvalidItem;
}
