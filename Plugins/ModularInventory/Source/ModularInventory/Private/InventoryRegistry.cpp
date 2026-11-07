#include "InventoryRegistry.h"

#include "ProfilingDebugging/CountersTrace.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"

TRACE_DECLARE_INT_COUNTER(InventoryItemCount, TEXT("Inventory/Items"));
TRACE_DECLARE_INT_COUNTER(InventoryContainerCount, TEXT("Inventory/Containers"));

void FInventoryChangeSet::Append(const FInventoryChangeSet& Other)
{
	Items.Append(Other.Items);
	Containers.Append(Other.Containers);
}

// Records
bool FInventoryRegistry::IsValid(FInventoryItemHandle Item) const
{
	return Items.IsValidIndex(Item.Index) && Items[Item.Index].bAlive && Items[Item.Index].Generation == Item.Generation;
}

bool FInventoryRegistry::IsValid(FInventoryContainerHandle Container) const
{
	return Containers.IsValidIndex(Container.Index) && Containers[Container.Index].bAlive && Containers[Container.Index].Generation == Container.Generation;
}

const FInventoryItemRecord* FInventoryRegistry::FindItem(FInventoryItemHandle Item) const
{
	return IsValid(Item) ? &Items[Item.Index] : nullptr;
}

const FInventoryContainerRecord* FInventoryRegistry::FindContainer(FInventoryContainerHandle Container) const
{
	return IsValid(Container) ? &Containers[Container.Index] : nullptr;
}

// Hierarchy
FInventoryContainerHandle FInventoryRegistry::GetRootContainer(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	while (Record)
	{
		const FInventoryItemRecord* OwnerItem = FindItem(Record->OwnerItem);
		if (!OwnerItem)
		{
			return Container;
		}
		Container = OwnerItem->Container;
		Record = FindContainer(Container);
	}
	return FInventoryContainerHandle();
}

UObject* FInventoryRegistry::GetOwner(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Root = FindContainer(GetRootContainer(Container));
	return Root ? Root->Owner.Get() : nullptr;
}

bool FInventoryRegistry::IsInsideItem(FInventoryContainerHandle Container, FInventoryItemHandle Item) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	while (Record && Record->OwnerItem.IsSet())
	{
		if (Record->OwnerItem == Item)
		{
			return true;
		}
		const FInventoryItemRecord* OwnerItem = FindItem(Record->OwnerItem);
		Record = OwnerItem ? FindContainer(OwnerItem->Container) : nullptr;
	}
	return false;
}

FInventoryContainerHandle FInventoryRegistry::FindItemContainer(FInventoryItemHandle Item, const FGameplayTag& Tag) const
{
	if (const FInventoryItemRecord* Record = FindItem(Item))
	{
		for (const FInventoryContainerHandle& Container : Record->Containers)
		{
			if (IsValid(Container) && Containers[Container.Index].Tag == Tag)
			{
				return Container;
			}
		}
	}
	return FInventoryContainerHandle();
}

void FInventoryRegistry::CollectContainers(TConstArrayView<FInventoryContainerHandle> Roots, TArray<FInventoryContainerHandle>& OutContainers) const
{
	TSet<FInventoryContainerHandle> Seen;
	const int32 FirstNew = OutContainers.Num();

	for (const FInventoryContainerHandle& Root : Roots)
	{
		if (IsValid(Root) && !Seen.Contains(Root))
		{
			Seen.Add(Root);
			OutContainers.Add(Root);
		}
	}

	// OutContainers grows while this walks it, which makes the walk breadth first.
	for (int32 Index = FirstNew; Index < OutContainers.Num(); ++Index)
	{
		for (const FInventoryItemHandle& Item : Containers[OutContainers[Index].Index].Items)
		{
			for (const FInventoryContainerHandle& Nested : Items[Item.Index].Containers)
			{
				if (IsValid(Nested) && !Seen.Contains(Nested))
				{
					Seen.Add(Nested);
					OutContainers.Add(Nested);
				}
			}
		}
	}
}

FInventoryItemHandle FInventoryRegistry::GetItemAt(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	const int32 ItemIndex = Record ? FindOccupant(*Record, Placement) : INDEX_NONE;
	return ItemIndex != INDEX_NONE ? MakeHandle(ItemIndex) : FInventoryItemHandle();
}

int32 FInventoryRegistry::FindOccupant(const FInventoryContainerRecord& Container, const FInventoryPlacement& Placement) const
{
	const UInventoryContainerLayout& Layout = *Container.Layout;
	switch (Layout.Type)
	{
	case EInventoryContainerType::Grid:
	{
		const FIntPoint& Cell = Placement.Cell;
		const bool bInside = Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Layout.GridSize.X && Cell.Y < Layout.GridSize.Y;
		const int32 CellIndex = Cell.Y * Layout.GridSize.X + Cell.X;
		return bInside && Container.Cells.IsValidIndex(CellIndex) ? Container.Cells[CellIndex] : INDEX_NONE;
	}
	case EInventoryContainerType::Slots:
		return Container.Cells.IsValidIndex(Placement.Index) ? Container.Cells[Placement.Index] : INDEX_NONE;
	case EInventoryContainerType::List:
		return Container.Items.IsValidIndex(Placement.Index) ? Container.Items[Placement.Index].Index : INDEX_NONE;
	}
	return INDEX_NONE;
}

// Transactions
EInventoryError FInventoryRegistry::RunTransaction(TFunctionRef<EInventoryError()> Body)
{
	BeginTransaction();
	const EInventoryError Error = Body();
	if (Error == EInventoryError::None)
	{
		CommitTransaction();
	}
	else
	{
		RollbackTransaction();
	}
	return Error;
}

EInventoryError FInventoryRegistry::DryRun(TFunctionRef<EInventoryError()> Body)
{
	BeginTransaction();
	const EInventoryError Error = Body();
	RollbackTransaction();
	return Error;
}

void FInventoryRegistry::BeginTransaction()
{
	FTransaction& Transaction = Transactions.AddDefaulted_GetRef();
	Transaction.NumItems = Items.Num();
	Transaction.NumContainers = Containers.Num();
	Transaction.FreeItems = FreeItems;
	Transaction.FreeContainers = FreeContainers;
}

void FInventoryRegistry::CommitTransaction()
{
	FTransaction Transaction = Transactions.Pop();

	if (!Transactions.IsEmpty())
	{
		// The parent keeps its own, older backups. Records it appended need none: its rollback drops them.
		FTransaction& Parent = Transactions.Last();
		for (TPair<int32, FInventoryItemRecord>& Backup : Transaction.ItemBackups)
		{
			if (Backup.Key < Parent.NumItems && !Parent.ItemBackups.Contains(Backup.Key))
			{
				Parent.ItemBackups.Add(Backup.Key, MoveTemp(Backup.Value));
			}
		}
		for (TPair<int32, FInventoryContainerRecord>& Backup : Transaction.ContainerBackups)
		{
			if (Backup.Key < Parent.NumContainers && !Parent.ContainerBackups.Contains(Backup.Key))
			{
				Parent.ContainerBackups.Add(Backup.Key, MoveTemp(Backup.Value));
			}
		}
		Parent.Changes.Append(Transaction.Changes);
		return;
	}

	UpdateTraceCounters();
	if (!Transaction.Changes.IsEmpty())
	{
		OnChanged.Broadcast(Transaction.Changes);
	}
}

void FInventoryRegistry::RollbackTransaction()
{
	FTransaction Transaction = Transactions.Pop();

	for (TPair<int32, FInventoryItemRecord>& Backup : Transaction.ItemBackups)
	{
		Items[Backup.Key] = MoveTemp(Backup.Value);
	}
	for (TPair<int32, FInventoryContainerRecord>& Backup : Transaction.ContainerBackups)
	{
		Containers[Backup.Key] = MoveTemp(Backup.Value);
	}

	Items.SetNum(Transaction.NumItems);
	Containers.SetNum(Transaction.NumContainers);
	FreeItems = MoveTemp(Transaction.FreeItems);
	FreeContainers = MoveTemp(Transaction.FreeContainers);
}

void FInventoryRegistry::UpdateTraceCounters() const
{
	TRACE_COUNTER_SET(InventoryItemCount, GetNumItems());
	TRACE_COUNTER_SET(InventoryContainerCount, GetNumContainers());
}

// Record editing
FInventoryItemRecord& FInventoryRegistry::EditItem(int32 Index)
{
	check(IsInTransaction());
	FTransaction& Transaction = Transactions.Last();
	if (Index < Transaction.NumItems && !Transaction.ItemBackups.Contains(Index))
	{
		Transaction.ItemBackups.Add(Index, Items[Index]);
	}

	FInventoryItemRecord& Record = Items[Index];
	Transaction.Changes.Items.Add(FInventoryItemHandle(Index, Record.Generation));
	if (Record.Container.IsSet())
	{
		Transaction.Changes.Containers.Add(Record.Container);
	}
	return Record;
}

FInventoryContainerRecord& FInventoryRegistry::EditContainer(int32 Index)
{
	check(IsInTransaction());
	FTransaction& Transaction = Transactions.Last();
	if (Index < Transaction.NumContainers && !Transaction.ContainerBackups.Contains(Index))
	{
		Transaction.ContainerBackups.Add(Index, Containers[Index]);
	}

	FInventoryContainerRecord& Record = Containers[Index];
	Transaction.Changes.Containers.Add(FInventoryContainerHandle(Index, Record.Generation));
	return Record;
}

int32 FInventoryRegistry::AllocateItem()
{
	const int32 Index = FreeItems.IsEmpty() ? Items.AddDefaulted() : FreeItems.Pop();
	EditItem(Index).bAlive = true;
	return Index;
}

int32 FInventoryRegistry::AllocateContainer()
{
	const int32 Index = FreeContainers.IsEmpty() ? Containers.AddDefaulted() : FreeContainers.Pop();
	EditContainer(Index).bAlive = true;
	return Index;
}

void FInventoryRegistry::FreeItem(int32 Index)
{
	FInventoryItemRecord& Record = EditItem(Index);
	const int32 NextGeneration = Record.Generation + 1;
	Record = FInventoryItemRecord();
	Record.Generation = NextGeneration;
	FreeItems.Add(Index);
}

void FInventoryRegistry::FreeContainer(int32 Index)
{
	FInventoryContainerRecord& Record = EditContainer(Index);
	const int32 NextGeneration = Record.Generation + 1;
	Record = FInventoryContainerRecord();
	Record.Generation = NextGeneration;
	FreeContainers.Add(Index);
}

// Creation and destruction
int32 FInventoryRegistry::CreateItem(FInventoryItemRecord&& Record)
{
	const int32 Index = AllocateItem();
	Record.Generation = Items[Index].Generation;
	Record.bAlive = true;
	Record.Container.Reset();
	Record.Containers.Reset();
	Items[Index] = MoveTemp(Record);

	const FInventoryItemHandle Handle = MakeHandle(Index);
	for (const FInventoryItemContainer& ContainerSpec : Items[Index].Definition->Containers)
	{
		if (ContainerSpec.Layout)
		{
			const FInventoryContainerHandle Container = CreateContainerRecord(ContainerSpec.Layout, ContainerSpec.Tag, nullptr, Handle);
			Items[Index].Containers.Add(Container);
		}
	}
	return Index;
}

FInventoryContainerHandle FInventoryRegistry::CreateContainerRecord(const UInventoryContainerLayout* Layout, const FGameplayTag& Tag, UObject* Owner, FInventoryItemHandle OwnerItem)
{
	const int32 Index = AllocateContainer();
	FInventoryContainerRecord& Record = Containers[Index];
	Record.Layout = Layout;
	Record.Tag = Tag;
	Record.Owner = Owner;
	Record.OwnerItem = OwnerItem;
	Record.Fragments = Layout->InstanceFragments;
	Record.Cells.Init(INDEX_NONE, Layout->Type == EInventoryContainerType::Grid ? Layout->GetCellCount() : Layout->GetSlotCount());
	return MakeContainerHandle(Index);
}

void FInventoryRegistry::DestroyItemRecord(int32 Index)
{
	const TArray<FInventoryContainerHandle> OwnedContainers = Items[Index].Containers;
	for (const FInventoryContainerHandle& Container : OwnedContainers)
	{
		if (IsValid(Container))
		{
			DestroyContainerRecord(Container.Index);
		}
	}

	DetachItem(Index);
	FreeItem(Index);
}

void FInventoryRegistry::DestroyContainerRecord(int32 Index)
{
	const TArray<FInventoryItemHandle> Contents = Containers[Index].Items;
	for (const FInventoryItemHandle& Item : Contents)
	{
		if (IsValid(Item))
		{
			DestroyItemRecord(Item.Index);
		}
	}

	FreeContainer(Index);
}

FInventoryContainerHandle FInventoryRegistry::CreateContainer(const UInventoryContainerLayout* Layout, const FGameplayTag& Tag, UObject* Owner)
{
	FInventoryContainerHandle Result;
	if (Layout)
	{
		RunTransaction([&]()
		{
			Result = CreateContainerRecord(Layout, Tag, Owner, FInventoryItemHandle());
			return EInventoryError::None;
		});
	}
	return Result;
}

EInventoryError FInventoryRegistry::DestroyContainer(FInventoryContainerHandle Container)
{
	const FInventoryContainerRecord* Record = FindContainer(Container);
	if (!Record || Record->OwnerItem.IsSet())
	{
		return EInventoryError::InvalidContainer;
	}

	return RunTransaction([&]()
	{
		DestroyContainerRecord(Container.Index);
		return EInventoryError::None;
	});
}

// Attachment
void FInventoryRegistry::AttachItem(int32 ItemIndex, FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	FInventoryContainerRecord& ContainerRecord = EditContainer(Container.Index);
	const FInventoryItemHandle Handle = MakeHandle(ItemIndex);

	// Only keep the fields the container type uses, so equal placements compare equal.
	FInventoryPlacement StoredPlacement;
	switch (ContainerRecord.Layout->Type)
	{
	case EInventoryContainerType::Grid:
		StoredPlacement.Cell = Placement.Cell;
		StoredPlacement.bRotated = Placement.bRotated;
		ContainerRecord.Items.Add(Handle);
		break;
	case EInventoryContainerType::Slots:
		StoredPlacement.Index = Placement.Index;
		ContainerRecord.Items.Add(Handle);
		break;
	case EInventoryContainerType::List:
		ContainerRecord.Items.Insert(Handle, FMath::Clamp(Placement.Index, 0, ContainerRecord.Items.Num()));
		break;
	}

	FInventoryItemRecord& ItemRecord = EditItem(ItemIndex);
	ItemRecord.Container = Container;
	ItemRecord.Placement = StoredPlacement;
	StampCells(Container.Index, ItemIndex, ItemIndex);
}

void FInventoryRegistry::DetachItem(int32 ItemIndex)
{
	const FInventoryContainerHandle Container = Items[ItemIndex].Container;
	if (!IsValid(Container))
	{
		return;
	}

	StampCells(Container.Index, ItemIndex, INDEX_NONE);
	EditContainer(Container.Index).Items.RemoveSingle(MakeHandle(ItemIndex));

	FInventoryItemRecord& ItemRecord = EditItem(ItemIndex);
	ItemRecord.Container.Reset();
	ItemRecord.Placement.Cell = FIntPoint::ZeroValue;
	ItemRecord.Placement.Index = 0;
}

void FInventoryRegistry::StampCells(int32 ContainerIndex, int32 ItemIndex, int32 Value)
{
	FInventoryContainerRecord& Container = EditContainer(ContainerIndex);
	const FInventoryItemRecord& Item = Items[ItemIndex];
	const UInventoryContainerLayout& Layout = *Container.Layout;

	if (Layout.Type == EInventoryContainerType::Grid)
	{
		const FIntPoint Size = Item.Definition->GetSize(Item.Placement.bRotated);
		for (int32 Y = Item.Placement.Cell.Y; Y < Item.Placement.Cell.Y + Size.Y; ++Y)
		{
			for (int32 X = Item.Placement.Cell.X; X < Item.Placement.Cell.X + Size.X; ++X)
			{
				const int32 CellIndex = Y * Layout.GridSize.X + X;
				if (Container.Cells.IsValidIndex(CellIndex))
				{
					Container.Cells[CellIndex] = Value;
				}
			}
		}
	}
	else if (Layout.Type == EInventoryContainerType::Slots && Container.Cells.IsValidIndex(Item.Placement.Index))
	{
		Container.Cells[Item.Placement.Index] = Value;
	}
}
