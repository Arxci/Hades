#include "InventoryUISubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventoryRegistry.h"
#include "InventorySubsystem.h"

namespace
{
	/** Whether the player may take items out of the container, or move them around inside it, as its layout's Player Access says. */
	bool CanPlayerTakeOut(const FInventoryRegistry& Registry, FInventoryContainerHandle Container)
	{
		const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
		return Record && Record->Layout->CanPlayerTakeOut();
	}

	/**
	 * Whether the player may put an item from From into the container. Moving an item around inside the container it's in only needs
	 * leave to take it out, so a take-only container can still be rearranged.
	 */
	bool CanPlayerPutIn(const FInventoryRegistry& Registry, FInventoryContainerHandle Container, FInventoryContainerHandle From)
	{
		const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
		return Record && (Record->Layout->CanPlayerPutIn() || (Container == From && Record->Layout->CanPlayerTakeOut()));
	}

	bool IsGrid(const FInventoryRegistry& Registry, FInventoryContainerHandle Container)
	{
		const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
		return Record && Record->Layout->Type == EInventoryContainerType::Grid;
	}

	FInventoryContainerHandle GetItemContainer(const FInventoryRegistry& Registry, FInventoryItemHandle Item)
	{
		const FInventoryItemRecord* Record = Registry.FindItem(Item);
		return Record ? Record->Container : FInventoryContainerHandle();
	}

	/** The item's middle cell from its top-left, rounded toward the top-left: what it's held by when nothing says otherwise. */
	FIntPoint GetMiddleCell(const FInventoryItemRecord& Record)
	{
		return (Record.Definition->GetSize(Record.Placement.bRotated) - FIntPoint(1, 1)) / 2;
	}

	/**
	 * The part of a grid a held item can go in: all its columns, and its rows down to the last with an unlocked cell. Cells unlock row by
	 * row, so the rows below that are wholly locked, or not there yet, and held items keep out of them as they keep inside the grid's edges.
	 * A grid that grows to fit also has free rows below its last, as many as Held Rows, the held item's height, for it to grow into.
	 */
	FIntPoint GetUsableGridSize(const FInventoryRegistry& Registry, FInventoryContainerHandle Container, const UInventoryContainerLayout& Layout, int32 HeldRows)
	{
		const int32 Columns = FMath::Max(0, Layout.GridSize.X);
		if (Columns == 0)
		{
			return FIntPoint::ZeroValue;
		}

		int32 Rows = Registry.GetGridRows(Container);
		if (Layout.GridGrowth == EInventoryGridGrowth::ToFitItems)
		{
			Rows += FMath::Max(1, HeldRows);
		}

		// Never past the last row with an unlocked cell, nor Max Rows.
		const int32 Capacity = Registry.GetCapacity(Container);
		if (Capacity != INDEX_NONE)
		{
			Rows = FMath::Min(Rows, FMath::DivideAndRoundUp(FMath::Max(0, Capacity), Columns));
		}
		const int32 MaxRows = Layout.GetMaxRows();
		if (MaxRows != INDEX_NONE)
		{
			Rows = FMath::Min(Rows, MaxRows);
		}
		return FIntPoint(Columns, Rows);
	}
}

UInventoryUISubsystem* UInventoryUISubsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UInventoryUISubsystem>() : nullptr;
}

void UInventoryUISubsystem::Deinitialize()
{
	RegistryListener.Unbind();

	Super::Deinitialize();
}

FInventoryRegistry* UInventoryUISubsystem::GetRegistry() const
{
	UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetLocalPlayer());
	return Inventory ? &Inventory->GetRegistry() : nullptr;
}

// Style
void UInventoryUISubsystem::SetStyle(const FInventoryInteractionStyle& InStyle)
{
	Style = InStyle;
	RefreshTarget();
}

// Holding
bool UInventoryUISubsystem::BeginHold(FInventoryItemHandle Item, int32 Count)
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(Item) : nullptr;
	return Record && BeginHoldBy(Item, Count, GetTargetedCell(Item).Get(GetMiddleCell(*Record)));
}

bool UInventoryUISubsystem::BeginHoldBy(FInventoryItemHandle Item, int32 Count, const FIntPoint& GrabCell, bool bAnnounce)
{
	if (!CanBeginHold(Item))
	{
		return false;
	}

	const FInventoryItemRecord* Record = GetRegistry()->FindItem(Item);
	const bool bWholeStack = Count <= 0 || Count >= Record->Count || !Record->Definition->CanSplit();
	const FIntPoint LastCell = Record->Definition->GetSize(Record->Placement.bRotated) - FIntPoint(1, 1);
	HeldGrabCell = GrabCell.ComponentMax(FIntPoint::ZeroValue).ComponentMin(LastCell);
	SetHeld(Item, bWholeStack ? Record->Count : Count, Record->Placement.bRotated);
	if (bAnnounce)
	{
		OnHoldEvent.Broadcast(EInventoryHoldEvent::PickedUp);
	}
	return true;
}

TOptional<FIntPoint> UInventoryUISubsystem::GetTargetedCell(FInventoryItemHandle Item) const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(Item) : nullptr;
	if (!Record || TargetContainer != Record->Container || !IsGrid(*Registry, Record->Container))
	{
		return TOptional<FIntPoint>();
	}

	const FIntPoint Cell = TargetPlacement.Cell - Record->Placement.Cell;
	const FIntPoint Size = Record->Definition->GetSize(Record->Placement.bRotated);
	const bool bOnItem = Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Size.X && Cell.Y < Size.Y;
	return bOnItem ? TOptional<FIntPoint>(Cell) : TOptional<FIntPoint>();
}

bool UInventoryUISubsystem::CanBeginHold(FInventoryItemHandle Item) const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(Item) : nullptr;
	return Record && !bChoosingItem && CanPlayerTakeOut(*Registry, Record->Container);
}

void UInventoryUISubsystem::CancelHold()
{
	if (IsHolding())
	{
		const FInventoryItemHandle Item = GetHeldItem();
		EndHold();
		OnHoldEvent.Broadcast(EInventoryHoldEvent::PutBack);
		OnItemPutDown.Broadcast(Item, EInventoryHoldEvent::PutBack);
	}
}

void UInventoryUISubsystem::EndHold()
{
	if (IsHolding())
	{
		SetHeld(FInventoryItemHandle(), 0, false);
	}
}

void UInventoryUISubsystem::RotateHeld()
{
	if (!CanRotateHeld())
	{
		return;
	}

	// Turned items are a quarter turn clockwise, so the cell it's held by turns with it, and stays where it's aimed.
	const FIntPoint Size = GetHeldSize();
	HeldGrabCell = bHeldRotated
		? FIntPoint(HeldGrabCell.Y, Size.X - 1 - HeldGrabCell.X)
		: FIntPoint(Size.Y - 1 - HeldGrabCell.Y, HeldGrabCell.X);
	SetHeld(HeldItem, HeldCount, !bHeldRotated);
	OnHoldEvent.Broadcast(EInventoryHoldEvent::Rotated);
}

bool UInventoryUISubsystem::CanRotateHeld() const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr;
	if (!Record || !Record->Definition->bCanRotate)
	{
		return false;
	}

	// Never into a shape the grid it's over can't hold at all, such as a long item stood upright in a short grid, counting only the rows
	// with unlocked cells.
	const FInventoryContainerHandle GridHandle = TargetContainer.IsSet() ? TargetContainer : Record->Container;
	const FInventoryContainerRecord* Grid = Registry->FindContainer(GridHandle);
	const FIntPoint TurnedSize = Record->Definition->GetSize(!bHeldRotated);
	if (!Grid || Grid->Layout->Type != EInventoryContainerType::Grid)
	{
		return true;
	}
	const FIntPoint UsableSize = GetUsableGridSize(*Registry, GridHandle, *Grid->Layout, TurnedSize.Y);
	return TurnedSize.X <= UsableSize.X && TurnedSize.Y <= UsableSize.Y;
}

void UInventoryUISubsystem::SplitHeld()
{
	const FInventoryRegistry* Registry = GetRegistry();
	if (const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr)
	{
		SetHeldCount(HeldCount > 1 ? HeldCount / 2 : Record->Count);
	}
}

bool UInventoryUISubsystem::CanSplitHeld() const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr;
	return Record && Record->Count > 1 && Record->Definition->CanSplit();
}

void UInventoryUISubsystem::SetHeldCount(int32 Count)
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr;
	if (!Record)
	{
		return;
	}

	const int32 NewCount = Record->Definition->CanSplit() ? FMath::Clamp(Count, 1, Record->Count) : Record->Count;
	if (NewCount != HeldCount)
	{
		SetHeld(HeldItem, NewCount, bHeldRotated);
		OnHoldEvent.Broadcast(EInventoryHoldEvent::Split);
	}
}

FIntPoint UInventoryUISubsystem::GetHeldSize() const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr;
	return Record ? Record->Definition->GetSize(bHeldRotated) : FIntPoint::ZeroValue;
}

int32 UInventoryUISubsystem::GetGridRowsShown(FInventoryContainerHandle Container) const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryContainerRecord* Record = Registry ? Registry->FindContainer(Container) : nullptr;
	if (!Record || Record->Layout->Type != EInventoryContainerType::Grid)
	{
		return 0;
	}

	const UInventoryContainerLayout& Layout = *Record->Layout;
	const int32 Rows = Registry->GetGridRows(Container);
	if (Layout.GridGrowth != EInventoryGridGrowth::ToFitItems)
	{
		return Rows;
	}

	// Room below the last row to put the held item down, which the grid then grows to fit, but none past Max Rows or the capacity.
	const int32 FreeRows = IsHolding() ? FMath::Max(1, GetHeldSize().Y) : 1;
	int32 Shown = Rows + FreeRows;
	const int32 MaxRows = Layout.GetMaxRows();
	if (MaxRows != INDEX_NONE)
	{
		Shown = FMath::Min(Shown, MaxRows);
	}
	const int32 Capacity = Registry->GetCapacity(Container);
	if (Capacity != INDEX_NONE && Layout.GridSize.X > 0)
	{
		Shown = FMath::Min(Shown, FMath::DivideAndRoundUp(FMath::Max(0, Capacity), Layout.GridSize.X));
	}
	return FMath::Max(Rows, Shown);
}

void UInventoryUISubsystem::SetHeld(FInventoryItemHandle Item, int32 Count, bool bRotated)
{
	const bool bWasHolding = IsHolding();
	HeldItem = Item;
	HeldCount = Count;
	bHeldRotated = bRotated;

	if (IsHolding() && !bWasHolding)
	{
		RegistryListener.Bind(GetLocalPlayer(), FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	}
	else if (!IsHolding())
	{
		RegistryListener.Unbind();
		HeldGrabCell = FIntPoint::ZeroValue;
	}

	RefreshTarget();
	OnHeldItemChanged.Broadcast();
}

void UInventoryUISubsystem::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr;
	if (!Record)
	{
		EndHold();
	}
	else if (HeldCount > Record->Count)
	{
		SetHeld(HeldItem, Record->Count, bHeldRotated);
	}
	else
	{
		RefreshTarget();
	}
}

// Placing
bool UInventoryUISubsystem::PlaceHeld(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryPlacement Landing = FitInGrid(Container, Placement);
	const FInventoryItemHandle Other = Registry ? Registry->GetItemAt(Container, Landing) : FInventoryItemHandle();
	const EInventoryDropAction Action = ResolvePlacement(Container, Placement, true);
	if (Action == EInventoryDropAction::Invalid)
	{
		OnHoldEvent.Broadcast(EInventoryHoldEvent::Refused);
	}
	if (Action == EInventoryDropAction::None || Action == EInventoryDropAction::Invalid)
	{
		return false;
	}

	// Items aren't drawn hovered while one is held, but the hover still follows the cursor over them. Before that shows again, it moves
	// to what was just put down, or the stack it joined, so the last item passed over doesn't light up for a moment.
	const bool bPickUpOther = Action == EInventoryDropAction::Swap && Style.bSwapPicksUpOther && Other.IsSet();
	const FInventoryItemHandle Landed = Registry ? Registry->GetItemAt(Container, Landing) : FInventoryItemHandle();
	if (!bPickUpOther)
	{
		SetHoveredItem(Landed);
	}

	// The other item had the held one's cells, so it's held by the same cell, and is drawn where it was until the cursor or focus moves on.
	const FIntPoint GrabCell = HeldGrabCell;
	EndHold();
	if (bPickUpOther)
	{
		BeginHoldBy(Other, 0, GrabCell, false);
	}
	const EInventoryHoldEvent Event = Action == EInventoryDropAction::Merge ? EInventoryHoldEvent::Merged
		: Action == EInventoryDropAction::Swap ? EInventoryHoldEvent::Swapped
		: EInventoryHoldEvent::Placed;
	OnHoldEvent.Broadcast(Event);
	if (Landed.IsSet())
	{
		OnItemPutDown.Broadcast(Landed, Event);
	}
	return true;
}

bool UInventoryUISubsystem::PressPlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryItemPress Press)
{
	if (IsHolding())
	{
		// Where grids draw it, when they do.
		const bool bAtTarget = Style.bSnapHeldItem && TargetContainer.IsSet();
		return PlaceHeld(bAtTarget ? TargetContainer : Container, bAtTarget ? TargetPlacement : Placement);
	}

	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemHandle Item = Registry ? Registry->GetItemAt(Container, Placement) : FInventoryItemHandle();
	if (!Item.IsSet())
	{
		return false;
	}

	// Choosing comes first, whatever the press would do otherwise, so the item asked for can always be chosen.
	if (bChoosingItem)
	{
		// An item can't be used on itself, so the choosing goes on.
		if (Item == ChoosingFor)
		{
			return false;
		}
		const FInventoryItemHandle For = ChoosingFor;
		CancelChoosingItem();
		OnItemChosen.Broadcast(For, Item);
		return true;
	}

	switch (Press)
	{
	case EInventoryItemPress::OpenItemMenu:
		RequestItemMenu(Item);
		return true;
	case EInventoryItemPress::PickUp:
		// Held by the cell pressed.
		SetTarget(Container, Placement);
		return BeginHold(Item);
	case EInventoryItemPress::DefaultAction:
		RequestDefaultAction(Item);
		return true;
	case EInventoryItemPress::None:
		break;
	}
	return false;
}

bool UInventoryUISubsystem::ActivatePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	return PressPlacement(Container, Placement, Style.Accept);
}

bool UInventoryUISubsystem::PickUpOrPlace()
{
	if (IsHolding())
	{
		return PlaceHeld(TargetContainer, TargetPlacement);
	}

	const FInventoryItemHandle Item = HoveredItem;
	if (!CanBeginHold(Item))
	{
		return false;
	}

	// Held by the cell aimed at, such as the focused one. Otherwise by its middle cell, aimed where it lies, so it's drawn there until
	// the cursor or focus moves on.
	if (!GetTargetedCell(Item).IsSet())
	{
		const FInventoryRegistry* Registry = GetRegistry();
		const FInventoryItemRecord* Record = Registry->FindItem(Item);
		const bool bInGrid = IsGrid(*Registry, Record->Container);
		SetTarget(Record->Container, bInGrid ? FInventoryPlacement::AtCell(Record->Placement.Cell + GetMiddleCell(*Record)) : Record->Placement);
	}
	return BeginHold(Item);
}

bool UInventoryUISubsystem::CanPickUpOrPlace() const
{
	return IsHolding() || CanBeginHold(HoveredItem);
}

bool UInventoryUISubsystem::ActivateAimed()
{
	if (IsHolding())
	{
		return PlaceHeld(TargetContainer, TargetPlacement);
	}

	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HoveredItem) : nullptr;
	if (!Record)
	{
		return false;
	}

	// On the cell aimed at, such as the focused one, so an item picked up is held by it, as with a click. Otherwise on its middle cell.
	if (GetTargetedCell(HoveredItem).IsSet())
	{
		return ActivatePlacement(TargetContainer, TargetPlacement);
	}
	const bool bInGrid = IsGrid(*Registry, Record->Container);
	return ActivatePlacement(Record->Container, bInGrid ? FInventoryPlacement::AtCell(Record->Placement.Cell + GetMiddleCell(*Record)) : Record->Placement);
}

bool UInventoryUISubsystem::CanActivateAimed() const
{
	if (IsHolding())
	{
		return true;
	}

	const FInventoryRegistry* Registry = GetRegistry();
	if (!Registry || !Registry->FindItem(HoveredItem))
	{
		return false;
	}
	if (bChoosingItem)
	{
		return HoveredItem != ChoosingFor;
	}
	switch (Style.Accept)
	{
	case EInventoryItemPress::OpenItemMenu:  return true;
	case EInventoryItemPress::DefaultAction: return true;
	case EInventoryItemPress::PickUp:        return CanBeginHold(HoveredItem);
	case EInventoryItemPress::None:          break;
	}
	return false;
}

EInventoryDropAction UInventoryUISubsystem::PreviewPlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	return ResolvePlacement(Container, Placement, false);
}

FInventoryPlacement UInventoryUISubsystem::FitInGrid(FInventoryContainerHandle Container, const FInventoryPlacement& Placement) const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryContainerRecord* Record = Registry ? Registry->FindContainer(Container) : nullptr;
	if (!Record || Record->Layout->Type != EInventoryContainerType::Grid || !IsHolding())
	{
		return Placement;
	}

	// The cell it's held by goes on the cell aimed at, moved in from the edges so it fits, rows wholly locked counting as past the edge.
	const FIntPoint HeldSize = GetHeldSize();
	const FIntPoint LastCell = (GetUsableGridSize(*Registry, Container, *Record->Layout, HeldSize.Y) - HeldSize).ComponentMax(FIntPoint::ZeroValue);
	FInventoryPlacement Fitted = Placement;
	Fitted.Cell = (Placement.Cell - HeldGrabCell).ComponentMax(FIntPoint::ZeroValue).ComponentMin(LastCell);

	// Over one other item of its size, even partly, it lands exactly in that item's place, to swap with it. Its own cells don't
	// count, or a long item couldn't shift along a cell.
	FInventoryItemHandle Other;
	for (int32 Y = 0; Y < HeldSize.Y; ++Y)
	{
		for (int32 X = 0; X < HeldSize.X; ++X)
		{
			const FInventoryItemHandle Under = Registry->GetItemAt(Container, FInventoryPlacement::AtCell(Fitted.Cell + FIntPoint(X, Y)));
			if (!Under.IsSet() || Under == HeldItem || Under == Other)
			{
				continue;
			}
			if (Other.IsSet())
			{
				return Fitted;
			}
			Other = Under;
		}
	}

	const FInventoryItemRecord* OtherRecord = Registry->FindItem(Other);
	const bool bSameSize = OtherRecord && OtherRecord->Definition->GetSize(OtherRecord->Placement.bRotated) == HeldSize;
	return bSameSize ? OtherRecord->Placement : Fitted;
}

EInventoryDropAction UInventoryUISubsystem::ResolvePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement, bool bApply)
{
	EInventoryError Error = EInventoryError::None;
	return ResolvePlacement(Container, Placement, bApply, Error);
}

EInventoryDropAction UInventoryUISubsystem::ResolvePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement, bool bApply, EInventoryError& OutError)
{
	OutError = EInventoryError::None;
	FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Held = Registry ? Registry->FindItem(HeldItem) : nullptr;
	const FInventoryContainerRecord* TargetRecord = Registry ? Registry->FindContainer(Container) : nullptr;
	if (!Held || !TargetRecord)
	{
		return EInventoryDropAction::None;
	}
	// The target's Player Access may not let the player put anything there, such as key items, or a container to take loot from.
	if (!CanPlayerPutIn(*Registry, Container, Held->Container))
	{
		OutError = EInventoryError::Rejected;
		return EInventoryDropAction::Invalid;
	}

	FInventoryPlacement Target = FitInGrid(Container, Placement);
	Target.bRotated = bHeldRotated;
	const int32 MoveCount = HeldCount < Held->Count ? HeldCount : 0;
	const FInventoryItemHandle Occupant = Registry->GetItemAt(Container, Target);
	const FInventoryItemRecord* OccupantRecord = Registry->FindItem(Occupant);
	const bool bOccupiedByOther = OccupantRecord && Occupant != HeldItem;
	const bool bMerge = bOccupiedByOther && Registry->CanStack(HeldItem, Occupant);

	// Only whole stacks swap: the rest of a split stack would have nowhere to go. Each item lands where the other was, so in a grid
	// it must take the same cells; a long item turned while held no longer has the shape of the spot it left.
	const FIntPoint OccupantSize = OccupantRecord ? OccupantRecord->Definition->GetSize(OccupantRecord->Placement.bRotated) : FIntPoint::ZeroValue;
	const bool bFitsTarget = !IsGrid(*Registry, Container) || OccupantSize == GetHeldSize();
	const bool bFitsSource = !IsGrid(*Registry, Held->Container) || OccupantSize == Held->Definition->GetSize(Held->Placement.bRotated);

	// The other item leaves the target for where the held one was, so the player has to be let take it out and put it there.
	const bool bOccupantMayMove = CanPlayerTakeOut(*Registry, Container) && CanPlayerPutIn(*Registry, Held->Container, Container);
	const bool bCanSwap = Style.bSwapItems && bOccupiedByOther && !bMerge && MoveCount == 0 && bFitsTarget && bFitsSource && bOccupantMayMove;

	// Equipment slots make way like the equipment component does: what's in the way goes to the owner's storage.
	const UInventoryContainerLayout& TargetLayout = *TargetRecord->Layout;
	const bool bEquipmentSlot = TargetLayout.bEquipment && TargetLayout.Slots.IsValidIndex(Target.Index);
	const FGameplayTag EquipSlot = bEquipmentSlot ? TargetLayout.Slots[Target.Index].Tag : FGameplayTag();
	const UInventoryComponent* SlotOwner = bEquipmentSlot ? Cast<UInventoryComponent>(Registry->GetOwner(Container)) : nullptr;
	const TArray<FInventoryContainerHandle> Storage = SlotOwner ? SlotOwner->GetStorageContainers() : TArray<FInventoryContainerHandle>();

	auto MoveHeld = [&]() { return Registry->MoveItem(HeldItem, Container, Target, MoveCount); };

	// Each takes the other's spot, the held item turned as it's held.
	auto SwapHeld = [&]()
	{
		return Registry->RunTransaction([&]()
		{
			const EInventoryError Error = Registry->SwapItems(HeldItem, Occupant);
			return Error == EInventoryError::None ? MoveHeld() : Error;
		});
	};
	auto EquipHeld = [&]() { return Registry->EquipItem(HeldItem, Container, EquipSlot, Storage); };
	auto Attempt = [&](TFunctionRef<EInventoryError()> Operation) { return bApply ? Operation() : Registry->DryRun(Operation); };

	const EInventoryError MoveError = Attempt(MoveHeld);
	if (MoveError == EInventoryError::None)
	{
		return bMerge ? EInventoryDropAction::Merge : EInventoryDropAction::Place;
	}

	if (bCanSwap && Attempt(SwapHeld) == EInventoryError::None)
	{
		return EInventoryDropAction::Swap;
	}
	if (!Storage.IsEmpty() && MoveCount == 0 && Attempt(EquipHeld) == EInventoryError::None)
	{
		return EInventoryDropAction::Swap;
	}

	// Why it can't simply go there, such as another item in the way.
	OutError = MoveError;
	return EInventoryDropAction::Invalid;
}

// Target
void UInventoryUISubsystem::SetTarget(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	if (Container == TargetContainer && Placement == TargetPlacement)
	{
		return;
	}

	TargetContainer = Container;
	TargetPlacement = Placement;
	RefreshTarget();
}

void UInventoryUISubsystem::ClearTarget()
{
	if (TargetContainer.IsSet())
	{
		TargetContainer.Reset();
		TargetPlacement = FInventoryPlacement();
		RefreshTarget();
	}
}

void UInventoryUISubsystem::RefreshTarget()
{
	TargetError = EInventoryError::None;
	TargetAction = TargetContainer.IsSet() ? ResolvePlacement(TargetContainer, TargetPlacement, false, TargetError) : EInventoryDropAction::None;
	OnTargetChanged.Broadcast();
}

// Hover
void UInventoryUISubsystem::SetHoveredItem(FInventoryItemHandle Item)
{
	if (Item != HoveredItem)
	{
		HoveredItem = Item;
		OnHoveredItemChanged.Broadcast(Item);
	}
}

// Selection
void UInventoryUISubsystem::SetSelectedItem(FInventoryItemHandle Item)
{
	if (Item != SelectedItem)
	{
		SelectedItem = Item;
		OnSelectedItemChanged.Broadcast(Item);
	}
}

// Item menus and choosing
void UInventoryUISubsystem::RequestItemMenu(FInventoryItemHandle Item)
{
	if (Item.IsSet())
	{
		OnItemMenuRequested.Broadcast(Item);
	}
}

void UInventoryUISubsystem::RequestDefaultAction(FInventoryItemHandle Item)
{
	if (Item.IsSet())
	{
		OnDefaultActionRequested.Broadcast(Item);
	}
}

void UInventoryUISubsystem::BeginChoosingItem(FInventoryItemHandle Item)
{
	CancelHold();
	bChoosingItem = true;
	ChoosingFor = Item;
	SetSelectedItem(Item);
}

void UInventoryUISubsystem::CancelChoosingItem()
{
	if (bChoosingItem)
	{
		bChoosingItem = false;
		ChoosingFor.Reset();
		SetSelectedItem(FInventoryItemHandle());
	}
}

// Transfer
UInventoryComponent* UInventoryUISubsystem::GetPlayerInventory() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const APlayerController* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	return Controller ? UInventoryComponent::FindInventoryComponent(Controller->GetPawn()) : nullptr;
}

bool UInventoryUISubsystem::FindTransferDirection(FInventoryItemHandle Item, bool& bOutToPlayer) const
{
	bOutToPlayer = false;
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(Item) : nullptr;
	if (!Record || !GetPlayerInventory() || bChoosingItem || !Registry->IsValid(TransferContainer) || !CanPlayerTakeOut(*Registry, Record->Container))
	{
		return false;
	}

	// Taken from the other side, it goes wherever the player's inventory puts what it takes, as a pickup's items do. Given to the other
	// side, the player has to be let put it in there.
	bOutToPlayer = Registry->GetRootContainer(Record->Container) == Registry->GetRootContainer(TransferContainer);
	return bOutToPlayer || CanPlayerPutIn(*Registry, TransferContainer, Record->Container);
}

bool UInventoryUISubsystem::TransferItem(FInventoryItemHandle Item)
{
	FInventoryRegistry* Registry = GetRegistry();
	UInventoryComponent* PlayerInventory = GetPlayerInventory();
	bool bToPlayer = false;
	if (!Registry || !PlayerInventory || !FindTransferDirection(Item, bToPlayer))
	{
		return false;
	}

	if (Item == HeldItem)
	{
		EndHold();
	}
	const FInventoryContainerHandle HoveredIn = GetItemContainer(*Registry, HoveredItem);

	// Into the player's inventory as it takes items, so a capacity upgrade it applies as it's taken is used, not stored.
	EInventoryError Error = EInventoryError::None;
	if (bToPlayer)
	{
		PlayerInventory->TakeItem(Item, 0, true, Error);
	}
	else
	{
		Error = Registry->MoveItemToContainers(Item, MakeArrayView(&TransferContainer, 1), 0, true);
	}
	if (Error != EInventoryError::None)
	{
		return false;
	}

	// Gone from under the cursor or focus, unless only some of the stack fit.
	if (GetItemContainer(*Registry, HoveredItem) != HoveredIn)
	{
		SetHoveredItem(FInventoryItemHandle());
	}
	return true;
}

bool UInventoryUISubsystem::CanTransferItem(FInventoryItemHandle Item) const
{
	FInventoryRegistry* Registry = GetRegistry();
	const UInventoryComponent* PlayerInventory = GetPlayerInventory();
	bool bToPlayer = false;
	if (!Registry || !PlayerInventory || !FindTransferDirection(Item, bToPlayer))
	{
		return false;
	}
	if (bToPlayer)
	{
		return PlayerInventory->CanTakeItem(Item, 0, true);
	}
	return Registry->DryRun([&]() { return Registry->MoveItemToContainers(Item, MakeArrayView(&TransferContainer, 1), 0, true); }) == EInventoryError::None;
}

bool UInventoryUISubsystem::TakeAll()
{
	FInventoryRegistry* Registry = GetRegistry();
	UInventoryComponent* PlayerInventory = GetPlayerInventory();
	if (!CanTakeAll() || !PlayerInventory)
	{
		return false;
	}

	EndHold();
	const FInventoryContainerHandle HoveredIn = GetItemContainer(*Registry, HoveredItem);
	const bool bTookEverything = PlayerInventory->TakeAll(TransferContainer);
	if (GetItemContainer(*Registry, HoveredItem) != HoveredIn)
	{
		SetHoveredItem(FInventoryItemHandle());
	}
	return bTookEverything;
}

bool UInventoryUISubsystem::CanTakeAll() const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryContainerRecord* Record = Registry ? Registry->FindContainer(TransferContainer) : nullptr;
	return Record && !Record->Items.IsEmpty() && !bChoosingItem && CanPlayerTakeOut(*Registry, TransferContainer) && GetPlayerInventory();
}

void UInventoryUISubsystem::ClearInteraction()
{
	EndHold();
	CancelChoosingItem();
	ClearTarget();
	SetHoveredItem(FInventoryItemHandle());
	SetSelectedItem(FInventoryItemHandle());
}
