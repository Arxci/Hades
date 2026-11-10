#include "InventoryUISubsystem.h"

#include "Engine/LocalPlayer.h"

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

// Holding
bool UInventoryUISubsystem::BeginHold(FInventoryItemHandle Item, int32 Count)
{
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(Item) : nullptr;
	if (!Record || bChoosingItem || IsReadOnly(*Registry, Record->Container))
	{
		return false;
	}

	const bool bWholeStack = Count <= 0 || Count >= Record->Count || !Record->Definition->CanSplit();
	SetHeld(Item, bWholeStack ? Record->Count : Count, Record->Placement.bRotated);
	return true;
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
	if (Action == EInventoryDropAction::Swap && bSwapPicksUpOther && Other.IsSet())
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
		const bool bAtTarget = bSnapHeldItem && TargetContainer.IsSet();
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
		bChoosingItem = false;
		OnItemChosen.Broadcast(Item);
		return true;
	}
	if (bClickOpensItemMenu)
	{
		RequestContext(Item);
		return true;
	}
	return BeginHold(Item);
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

	// Only an item of the same size and shape swaps, so a swap never depends on what space is free. A long item turned while
	// held no longer has the shape of the spot it left, so it doesn't swap. Only whole stacks swap: the rest of a split stack
	// would have nowhere to go.
	const FIntPoint HeldSize = GetHeldSize();
	const bool bKeepsShape = bHeldRotated == Held->Placement.bRotated || HeldSize.X == HeldSize.Y;
	const bool bCanSwap = bOccupiedByOther && !bMerge && MoveCount == 0 && bKeepsShape
		&& OccupantRecord->Definition->GetSize(OccupantRecord->Placement.bRotated) == HeldSize;

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

void UInventoryUISubsystem::BeginChoosingItem()
{
	CancelHold();
	bChoosingItem = true;
}

void UInventoryUISubsystem::ClearInteraction()
{
	CancelHold();
	CancelChoosingItem();
	ClearTarget();
	SetHoveredItem(FInventoryItemHandle());
	SetSelectedItem(FInventoryItemHandle());
}
