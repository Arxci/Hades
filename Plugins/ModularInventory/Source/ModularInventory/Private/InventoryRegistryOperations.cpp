#include "InventoryRegistry.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"

namespace
{
	/**
	 * Why containers turned an item away, for operations that try several. When none would take it at all, the reason is what's
	 * reported, preferring a rule such as a carry limit over a filter; when one took it but had no room, it's the fallback, No Space.
	 */
	struct FEntryRefusals
	{
		bool bAnyEntered = false;
		EInventoryError First = EInventoryError::None;

		bool Note(EInventoryError Error)
		{
			bAnyEntered |= Error == EInventoryError::None;
			if (Error != EInventoryError::None && (First == EInventoryError::None || (First == EInventoryError::Rejected && Error != EInventoryError::Rejected)))
			{
				First = Error;
			}
			return Error == EInventoryError::None;
		}

		EInventoryError Reason(EInventoryError Fallback) const
		{
			return !bAnyEntered && First != EInventoryError::None ? First : Fallback;
		}
	};
}

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

		// Every cell it covers must be usable: inside the grid's columns, within its Max Rows, and unlocked. Rows a grid that grows to fit
		// doesn't have yet are free, as it adds them for the item.
		// A layout whose columns changed since the container was built, such as an asset edited while playing, would read its cells
		// wrongly, so nothing more is placed until it's given its layout again.
		const FIntPoint Size = Item.Definition->GetSize(Placement.bRotated);
		const FIntPoint& Cell = Placement.Cell;
		const bool bLayoutChanged = ContainerRecord->CellColumns != Layout.GridSize.X;
		if (bLayoutChanged || Cell.X < 0 || Cell.Y < 0 || Cell.X + Size.X > Layout.GridSize.X)
		{
			return EInventoryError::OutOfBounds;
		}

		for (int32 Y = Cell.Y; Y < Cell.Y + Size.Y; ++Y)
		{
			for (int32 X = Cell.X; X < Cell.X + Size.X; ++X)
			{
				if (!IsRecordCellUsable(*ContainerRecord, FIntPoint(X, Y)))
				{
					return EInventoryError::OutOfBounds;
				}

				const int32 CellIndex = GetCellIndex(Layout, FIntPoint(X, Y));
				const int32 Occupant = ContainerRecord->Cells.IsValidIndex(CellIndex) ? ContainerRecord->Cells[CellIndex] : INDEX_NONE;
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

		// Locked slots, past the container's usable slots, are outside the part that can be used.
		if (Placement.Index >= GetRecordCapacity(*ContainerRecord))
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
		const int32 Capacity = GetRecordCapacity(*ContainerRecord);
		const bool bAlreadyInside = Item.Container == Container;
		const bool bFull = Capacity != INDEX_NONE && ContainerRecord->Items.Num() >= Capacity;
		return bFull && !bAlreadyInside ? EInventoryError::NoSpace : EInventoryError::None;
	}
	}
	return EInventoryError::None;
}

bool FInventoryRegistry::IsPlacementUsable(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	return Record && IsRecordPlacementUsable(*Record, Placement);
}

int32 FInventoryRegistry::GetCapacity(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	return Record ? GetRecordCapacity(*Record) : 0;
}

int32 FInventoryRegistry::GetMaxCapacity(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	return Record ? Record->Layout->GetMaxCapacity() : 0;
}

int32 FInventoryRegistry::GetGridRows(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	return Record ? GetRecordGridRows(*Record) : 0;
}

int32 FInventoryRegistry::GetRecordGridRows(const FInventoryContainerRecord& Container)
{
	const UInventoryContainerLayout& Layout = *Container.Layout;
	const int32 Columns = Layout.GridSize.X;
	if (Layout.Type != EInventoryContainerType::Grid || Columns < 1)
	{
		return 0;
	}
	if (Layout.GridGrowth != EInventoryGridGrowth::ToFitItems)
	{
		return Layout.GetGridRowsAtCapacity(GetRecordCapacity(Container));
	}

	// Grown to fit: down to the last row anything is on, and never fewer than Grid Size's rows.
	int32 RowsUsed = 0;
	for (int32 Index = Container.Cells.Num() - 1; Index >= 0; --Index)
	{
		if (Container.Cells[Index] != INDEX_NONE)
		{
			RowsUsed = Index / Columns + 1;
			break;
		}
	}
	return FMath::Max(FMath::Max(0, Layout.GridSize.Y), RowsUsed);
}

int32 FInventoryRegistry::GetCellIndex(const UInventoryContainerLayout& Layout, const FIntPoint& Cell)
{
	const int32 Columns = Layout.GridSize.X;
	if (Layout.Type != EInventoryContainerType::Grid || Cell.X < 0 || Cell.Y < 0 || Cell.X >= Columns)
	{
		return INDEX_NONE;
	}
	const int64 Index = int64(Cell.Y) * Columns + Cell.X;
	return Index <= MAX_int32 ? static_cast<int32>(Index) : INDEX_NONE;
}

int32 FInventoryRegistry::GetRecordCapacity(const FInventoryContainerRecord& Container)
{
	// The container's own capacity, or else its layout's. Grids and slots can't hold more than they have, even with a layout swapped in.
	const UInventoryContainerLayout& Layout = *Container.Layout;
	return Container.CapacityOverride < 0 ? Layout.GetStartingCapacity() : Layout.ClampCapacity(Container.CapacityOverride);
}

bool FInventoryRegistry::IsRecordCellUsable(const FInventoryContainerRecord& Container, const FIntPoint& Cell)
{
	// Inside the grid's columns and within its Max Rows, if it has any.
	const UInventoryContainerLayout& Layout = *Container.Layout;
	const int32 CellIndex = GetCellIndex(Layout, Cell);
	const int32 MaxRows = Layout.GetMaxRows();
	if (CellIndex == INDEX_NONE || (MaxRows != INDEX_NONE && Cell.Y >= MaxRows))
	{
		return false;
	}

	// Cells unlock row by row from the top left, so a cell is usable while its index is within the capacity, and any is without a limit.
	const int32 Capacity = GetRecordCapacity(Container);
	return Capacity == INDEX_NONE || CellIndex < Capacity;
}

bool FInventoryRegistry::IsRecordPlacementUsable(const FInventoryContainerRecord& Container, const FInventoryPlacement& Placement)
{
	const UInventoryContainerLayout& Layout = *Container.Layout;
	switch (Layout.Type)
	{
	case EInventoryContainerType::Grid:
		return IsRecordCellUsable(Container, Placement.Cell);
	case EInventoryContainerType::Slots:
		return Layout.Slots.IsValidIndex(Placement.Index) && Placement.Index < GetRecordCapacity(Container);
	case EInventoryContainerType::List:
	{
		const int32 Capacity = GetRecordCapacity(Container);
		return Placement.Index >= 0 && (Capacity == INDEX_NONE || Placement.Index < Capacity);
	}
	}
	return false;
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
		// Row by row from the top left, in either orientation, within the rows the grid has. A grid that grows to fit then also tries spots
		// reaching into rows below its last, which are always free until Max Rows.
		const bool bPreferRotated = Item.Placement.bRotated;
		const int32 Rows = GetRecordGridRows(*ContainerRecord);
		const bool bCanAddRows = Layout.GridGrowth == EInventoryGridGrowth::ToFitItems;
		for (const bool bIntoNewRows : { false, true })
		{
			if (bIntoNewRows && !bCanAddRows)
			{
				break;
			}

			for (const bool bRotated : { bPreferRotated, !bPreferRotated })
			{
				if (bRotated && !Item.Definition->bCanRotate)
				{
					continue;
				}

				const FIntPoint Size = Item.Definition->GetSize(bRotated);
				const int32 FirstRow = bIntoNewRows ? FMath::Max(0, Rows - Size.Y + 1) : 0;
				const int32 LastRow = bIntoNewRows ? Rows : Rows - Size.Y;
				for (int32 Y = FirstRow; Y <= LastRow; ++Y)
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
	if (!IsValid(Container))
	{
		return EInventoryError::InvalidContainer;
	}

	// One stack, as big as the container lets it be, its stack bonuses counted.
	const FInventoryItemRecord Prototype = MakeItemRecord(Spec);
	const int32 MaxStack = GetRecordMaxStackSize(Prototype, Container);
	if (Spec.Count < 1 || Spec.Count > MaxStack)
	{
		return EInventoryError::InvalidCount;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		FInventoryItemRecord Record = Prototype;
		EInventoryError Error = CanEnter(INDEX_NONE, Record, Container);
		if (Error != EInventoryError::None)
		{
			return Error;
		}

		const int32 Occupant = FindOccupant(Containers[Container.Index], *Placement);
		if (Occupant != INDEX_NONE && RecordsCanStack(Record, Items[Occupant]))
		{
			if (Items[Occupant].Count + Spec.Count > MaxStack)
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

		const FInventoryItemRecord Prototype = MakeItemRecord(Spec);
		int32 Remaining = Spec.Count;
		FEntryRefusals Refusals;
		auto Enters = [&](FInventoryContainerHandle Container) { return Refusals.Note(CanEnter(INDEX_NONE, Prototype, Container)); };

		// Top up existing stacks before taking new space, each as high as its container lets it stack.
		for (const FInventoryContainerHandle& Container : TargetContainers)
		{
			const int32 MaxStack = GetRecordMaxStackSize(Prototype, Container);
			if (MaxStack <= 1 || Remaining == 0 || !Enters(Container))
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
				if (Enters(Container) && FindPlacementFor(INDEX_NONE, Prototype, Container, Placement))
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
			Record.Count = FMath::Min(Remaining, GetRecordMaxStackSize(Prototype, Target));
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
		return bAcceptable ? EInventoryError::None : Refusals.Reason(EInventoryError::NoSpace);
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
	const int32 Moved = FMath::Min(Requested, GetRecordMaxStackSize(*TargetRecord, TargetRecord->Container) - TargetRecord->Count);
	if (Moved <= 0)
	{
		return EInventoryError::CannotStack;
	}

	// Count moving into another container goes through its entry check, as any other move would, such as a carry limit.
	if (SourceRecord->Container != TargetRecord->Container)
	{
		FInventoryItemRecord Moving = *SourceRecord;
		Moving.Count = Moved;
		const EInventoryError EnterError = CanEnter(Source.Index, Moving, TargetRecord->Container);
		if (EnterError != EInventoryError::None)
		{
			return EnterError;
		}
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

	// As high as the target container lets it stack. A stack already bigger than that still moves whole.
	const int32 MaxStack = GetRecordMaxStackSize(Items[ItemIndex], Container);

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
	// The stacks already there are topped up as high as their container lets them stack.
	const int32 MaxStack = GetRecordMaxStackSize(Items[ItemIndex], Container);
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
		FEntryRefusals Refusals;
		auto Enters = [&](FInventoryContainerHandle Container) { return Refusals.Note(CanEnter(MovingIndex, Items[MovingIndex], Container)); };
		for (const FInventoryContainerHandle& Container : TargetContainers)
		{
			if (Enters(Container) && TopUpStacks(MovingIndex, Container))
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
			if (Enters(Container) && FindPlacementFor(MovingIndex, Items[MovingIndex], Container, FreeSpot))
			{
				DetachItem(MovingIndex);
				AttachItem(MovingIndex, Container, FreeSpot);
				return EInventoryError::None;
			}
		}

		const bool bSomethingMoved = Items[MovingIndex].Count < MovingCount;
		if (!bAllowPartial || !bSomethingMoved)
		{
			return TargetContainers.IsEmpty() ? EInventoryError::InvalidContainer : Refusals.Reason(EInventoryError::NoSpace);
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
		// A new shape comes with its own capacity, so what was unlocked on the old one doesn't carry over.
		FInventoryContainerRecord& Edited = EditContainer(Container.Index);
		Edited.Layout = Layout;
		Edited.CapacityOverride = INDEX_NONE;
		Edited.Cells.Init(INDEX_NONE, Layout->Type == EInventoryContainerType::Grid ? Layout->GetCellCount() : Layout->GetSlotCount());
		Edited.CellColumns = Layout->Type == EInventoryContainerType::Grid ? Layout->GridSize.X : 0;
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

EInventoryError FInventoryRegistry::SetCapacity(FInventoryContainerHandle Container, int32 Count)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_SetCapacity);

	const FInventoryContainerRecord* Record = FindContainer(Container);
	if (!Record)
	{
		return EInventoryError::InvalidContainer;
	}

	// Grids and slots can't unlock more than they have, so asking for more unlocks them all.
	const UInventoryContainerLayout& Layout = *Record->Layout;
	const int32 Override = Layout.ClampCapacity(Count);
	if (Override == Record->CapacityOverride)
	{
		return EInventoryError::None;
	}

	return RunTransaction([&]() -> EInventoryError
	{
		EditContainer(Container.Index).CapacityOverride = Override;

		// A list already holding more than that keeps its capacity: nothing changes.
		const FInventoryContainerRecord& Edited = Containers[Container.Index];
		const int32 Capacity = GetRecordCapacity(Edited);
		if (Layout.Type == EInventoryContainerType::List && Capacity != INDEX_NONE && Edited.Items.Num() > Capacity)
		{
			return EInventoryError::NoSpace;
		}

		// An item on a cell or in a slot this would lock keeps it unlocked: nothing changes.
		for (const FInventoryItemHandle& Item : Edited.Items)
		{
			const EInventoryError Error = CanOccupy(Item.Index, Items[Item.Index], Container, Items[Item.Index].Placement);
			if (Error != EInventoryError::None)
			{
				return Error;
			}
		}
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::AddCapacity(FInventoryContainerHandle Container, int32 Amount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_AddCapacity);

	const FInventoryContainerRecord* Record = FindContainer(Container);
	if (!Record)
	{
		return EInventoryError::InvalidContainer;
	}
	if (Amount == 0)
	{
		return EInventoryError::None;
	}

	// A list without a limit has nothing left to unlock, and no count to lock from.
	const int32 Current = GetRecordCapacity(*Record);
	if (Current == INDEX_NONE)
	{
		return Amount > 0 ? EInventoryError::AtMaxCapacity : EInventoryError::InvalidCount;
	}

	// Grids and slots stop at all they have; lists only at what an int holds. Nothing left to unlock or lock counts as failing, so an
	// upgrade can tell the container is already at its biggest.
	const UInventoryContainerLayout& Layout = *Record->Layout;
	const int64 Most = Layout.GetMaxCapacity() == INDEX_NONE ? int64(MAX_int32) : int64(Layout.GetMaxCapacity());
	const int32 Wanted = static_cast<int32>(FMath::Clamp(int64(Current) + Amount, int64(0), Most));
	if (Wanted == Current)
	{
		return Amount > 0 ? EInventoryError::AtMaxCapacity : EInventoryError::InvalidCount;
	}
	return SetCapacity(Container, Wanted);
}

EInventoryError FInventoryRegistry::AddStackBonus(FInventoryContainerHandle Container, const FInventoryItemQuery& ItemQuery, int32 Amount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_AddStackBonus);

	if (!IsValid(Container))
	{
		return EInventoryError::InvalidContainer;
	}
	if (Amount < 1)
	{
		return EInventoryError::InvalidCount;
	}

	return RunTransaction([&]()
	{
		FInventoryStackBonus& Bonus = EditContainer(Container.Index).StackBonuses.AddDefaulted_GetRef();
		Bonus.Items = ItemQuery;
		Bonus.Amount = Amount;
		return EInventoryError::None;
	});
}

EInventoryError FInventoryRegistry::SortContainer(FInventoryContainerHandle Container)
{
	return SortContainer(Container, FInventorySortSettings());
}

EInventoryError FInventoryRegistry::SortContainer(FInventoryContainerHandle Container, const FInventorySortSettings& Settings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Inventory_SortContainer);

	const FInventoryContainerRecord* Record = FindContainer(Container);
	if (!Record)
	{
		return EInventoryError::InvalidContainer;
	}
	const bool bGrid = Record->Layout->Type == EInventoryContainerType::Grid;
	if (!bGrid && Record->Layout->Type != EInventoryContainerType::List)
	{
		return EInventoryError::Rejected;
	}

	// Where an item's tags first appear in Tag Order, or past its end without any of them.
	auto GetTagRank = [this, &Settings](const FInventoryItemHandle& Item)
	{
		const FGameplayTagContainer Tags = GetItemTags(Item);
		const int32 Rank = Settings.TagOrder.IndexOfByPredicate([&Tags](const FGameplayTag& Tag) { return Tag.IsValid() && Tags.HasTag(Tag); });
		return Rank == INDEX_NONE ? Settings.TagOrder.Num() : Rank;
	};

	// Negative when A goes first, positive when B does, by the first key they differ by.
	auto Compare = [this, bGrid, &Settings, &GetTagRank](const FInventoryItemHandle& A, const FInventoryItemHandle& B) -> int32
	{
		const FInventoryItemRecord& RecordA = Items[A.Index];
		const FInventoryItemRecord& RecordB = Items[B.Index];
		const UInventoryItemDefinition& DefinitionA = *RecordA.Definition;
		const UInventoryItemDefinition& DefinitionB = *RecordB.Definition;
		for (const EInventorySortKey Key : Settings.Keys)
		{
			int32 Order = 0;
			switch (Key)
			{
			case EInventorySortKey::Size:
				Order = bGrid ? (DefinitionB.Size.X * DefinitionB.Size.Y) - (DefinitionA.Size.X * DefinitionA.Size.Y) : 0;
				break;
			case EInventorySortKey::Name:
				Order = DefinitionA.DisplayName.CompareTo(DefinitionB.DisplayName);
				break;
			case EInventorySortKey::Category:
				Order = DefinitionA.Category.CompareTo(DefinitionB.Category);
				break;
			case EInventorySortKey::TagOrder:
				Order = GetTagRank(A) - GetTagRank(B);
				break;
			case EInventorySortKey::TagValue:
				if (Settings.ValueTag.IsValid())
				{
					const bool bHasA = HasTagValue(A, Settings.ValueTag);
					const bool bHasB = HasTagValue(B, Settings.ValueTag);
					const float ValueA = bHasA ? GetTagValue(A, Settings.ValueTag) : 0.0f;
					const float ValueB = bHasB ? GetTagValue(B, Settings.ValueTag) : 0.0f;
					Order = bHasA != bHasB ? (bHasA ? -1 : 1) : ValueA > ValueB ? -1 : ValueA < ValueB ? 1 : 0;
				}
				break;
			case EInventorySortKey::Count:
				Order = RecordB.Count - RecordA.Count;
				break;
			}
			if (Order != 0)
			{
				return Settings.bReverse ? -Order : Order;
			}
		}
		return 0;
	};

	TArray<FInventoryItemHandle> Sorted = Record->Items;
	Sorted.StableSort([&Compare](const FInventoryItemHandle& A, const FInventoryItemHandle& B)
	{
		return Compare(A, B) < 0;
	});

	return RunTransaction([&]() -> EInventoryError
	{
		for (const FInventoryItemHandle& Item : Sorted)
		{
			DetachItem(Item.Index);
		}

		for (const FInventoryItemHandle& Item : Sorted)
		{
			if (Settings.bMergeStacks && TopUpStacks(Item.Index, Container))
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
