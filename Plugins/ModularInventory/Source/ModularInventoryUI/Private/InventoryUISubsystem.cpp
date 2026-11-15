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
	bool IsReadOnly(const FInventoryRegistry& Registry, FInventoryContainerHandle Container)
	{
		const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
		return Record && Record->Layout->bReadOnlyInUI;
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
	return Record && !bChoosingItem && !IsReadOnly(*Registry, Record->Container);
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

	// Never into a shape the grid it's over can't hold at all, such as a long gun stood upright in short pockets.
	const FInventoryContainerRecord* Grid = Registry->FindContainer(TargetContainer.IsSet() ? TargetContainer : Record->Container);
	const FIntPoint TurnedSize = Record->Definition->GetSize(!bHeldRotated);
	const bool bOverGrid = Grid && Grid->Layout->Type == EInventoryContainerType::Grid;
	return !bOverGrid || (TurnedSize.X <= Grid->Layout->GridSize.X && TurnedSize.Y <= Grid->Layout->GridSize.Y);
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

bool UInventoryUISubsystem::ActivatePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
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
	if (Style.bClickOpensItemMenu)
	{
		RequestContext(Item);
		return true;
	}

	// Held by the cell clicked or accepted on.
	SetTarget(Container, Placement);
	return BeginHold(Item);
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
	return Style.bClickOpensItemMenu || CanBeginHold(HoveredItem);
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

	// The cell it's held by goes on the cell aimed at, moved in from the edges so it fits.
	const FIntPoint HeldSize = GetHeldSize();
	const FIntPoint LastCell = (Record->Layout->GridSize - HeldSize).ComponentMax(FIntPoint::ZeroValue);
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
	if (TargetRecord->Layout->bReadOnlyInUI)
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
	const bool bCanSwap = Style.bSwapItems && bOccupiedByOther && !bMerge && MoveCount == 0 && bFitsTarget && bFitsSource;

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
void UInventoryUISubsystem::RequestContext(FInventoryItemHandle Item)
{
	if (Item.IsSet())
	{
		OnContextRequested.Broadcast(Item);
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

TArray<FInventoryContainerHandle> UInventoryUISubsystem::GetTransferTargets(FInventoryItemHandle Item) const
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(Item) : nullptr;
	const UInventoryComponent* PlayerInventory = GetPlayerInventory();
	if (!Record || !PlayerInventory || bChoosingItem || !Registry->IsValid(TransferContainer)
		|| IsReadOnly(*Registry, Record->Container) || IsReadOnly(*Registry, TransferContainer))
	{
		return TArray<FInventoryContainerHandle>();
	}

	const bool bFromTransfer = Registry->GetRootContainer(Record->Container) == Registry->GetRootContainer(TransferContainer);
	return bFromTransfer ? PlayerInventory->GetStorageContainers() : TArray<FInventoryContainerHandle>({ TransferContainer });
}

bool UInventoryUISubsystem::TransferItem(FInventoryItemHandle Item)
{
	FInventoryRegistry* Registry = GetRegistry();
	const TArray<FInventoryContainerHandle> Targets = GetTransferTargets(Item);
	if (!Registry || Targets.IsEmpty())
	{
		return false;
	}

	if (Item == HeldItem)
	{
		EndHold();
	}
	const FInventoryContainerHandle HoveredIn = GetItemContainer(*Registry, HoveredItem);
	if (Registry->MoveItemToContainers(Item, Targets, 0, true) != EInventoryError::None)
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
	const TArray<FInventoryContainerHandle> Targets = GetTransferTargets(Item);
	return Registry && !Targets.IsEmpty() && Registry->DryRun([&]() { return Registry->MoveItemToContainers(Item, Targets, 0, true); }) == EInventoryError::None;
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
	return Record && !Record->Items.IsEmpty() && !bChoosingItem && !IsReadOnly(*Registry, TransferContainer) && GetPlayerInventory();
}

void UInventoryUISubsystem::ClearInteraction()
{
	EndHold();
	CancelChoosingItem();
	ClearTarget();
	SetHoveredItem(FInventoryItemHandle());
	SetSelectedItem(FInventoryItemHandle());
}
