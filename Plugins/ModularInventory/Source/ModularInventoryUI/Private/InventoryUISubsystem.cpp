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
	if (!CanBeginHold(Item))
	{
		return false;
	}

	const FInventoryItemRecord* Record = GetRegistry()->FindItem(Item);
	const bool bWholeStack = Count <= 0 || Count >= Record->Count || !Record->Definition->CanSplit();
	SetHeld(Item, bWholeStack ? Record->Count : Count, Record->Placement.bRotated);
	return true;
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
		SetHeld(FInventoryItemHandle(), 0, false);
	}
}

void UInventoryUISubsystem::RotateHeld()
{
	if (CanRotateHeld())
	{
		SetHeld(HeldItem, HeldCount, !bHeldRotated);
	}
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
		CancelHold();
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
	const FInventoryItemHandle Other = Registry ? Registry->GetItemAt(Container, FitInGrid(Container, Placement)) : FInventoryItemHandle();
	const EInventoryDropAction Action = ResolvePlacement(Container, Placement, true);
	if (Action == EInventoryDropAction::None || Action == EInventoryDropAction::Invalid)
	{
		return false;
	}

	CancelHold();
	if (Action == EInventoryDropAction::Swap && Style.bSwapPicksUpOther && Other.IsSet())
	{
		BeginHold(Other);
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

	// Aimed where it lies, so it's drawn there until the cursor or focus moves on.
	const FInventoryRegistry* Registry = GetRegistry();
	SetTarget(Registry->FindItem(Item)->Container, Registry->GetItemPlacement(Item));
	return BeginHold(Item);
}

bool UInventoryUISubsystem::CanPickUpOrPlace() const
{
	return IsHolding() || CanBeginHold(HoveredItem);
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

	const FIntPoint HeldSize = GetHeldSize();
	const FIntPoint LastCell = (Record->Layout->GridSize - HeldSize).ComponentMax(FIntPoint::ZeroValue);
	FInventoryPlacement Fitted = Placement;
	Fitted.Cell = Placement.Cell.ComponentMax(FIntPoint::ZeroValue).ComponentMin(LastCell);

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
	FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Held = Registry ? Registry->FindItem(HeldItem) : nullptr;
	const FInventoryContainerRecord* TargetRecord = Registry ? Registry->FindContainer(Container) : nullptr;
	if (!Held || !TargetRecord)
	{
		return EInventoryDropAction::None;
	}
	if (TargetRecord->Layout->bReadOnlyInUI)
	{
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

	if (Attempt(MoveHeld) == EInventoryError::None)
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
	TargetAction = TargetContainer.IsSet() ? ResolvePlacement(TargetContainer, TargetPlacement, false) : EInventoryDropAction::None;
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
		CancelHold();
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

	CancelHold();
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
	CancelHold();
	CancelChoosingItem();
	ClearTarget();
	SetHoveredItem(FInventoryItemHandle());
	SetSelectedItem(FInventoryItemHandle());
}
