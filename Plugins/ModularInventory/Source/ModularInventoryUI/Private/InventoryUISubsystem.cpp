#include "InventoryUISubsystem.h"

#include "Engine/LocalPlayer.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventoryRegistry.h"
#include "InventorySubsystem.h"

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
	if (!Record || bChoosingItem)
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
	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemRecord* Record = Registry ? Registry->FindItem(HeldItem) : nullptr;
	if (Record && Record->Definition->bCanRotate)
	{
		SetHeld(HeldItem, HeldCount, !bHeldRotated);
	}
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
	const EInventoryDropAction Action = ResolvePlacement(Container, Placement, true);
	const bool bPlaced = Action != EInventoryDropAction::None && Action != EInventoryDropAction::Invalid;
	if (bPlaced)
	{
		CancelHold();
	}
	return bPlaced;
}

bool UInventoryUISubsystem::ActivatePlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	if (IsHolding())
	{
		return PlaceHeld(Container, Placement);
	}

	const FInventoryRegistry* Registry = GetRegistry();
	const FInventoryItemHandle Item = Registry ? Registry->GetItemAt(Container, Placement) : FInventoryItemHandle();
	if (bChoosingItem && Item.IsSet())
	{
		bChoosingItem = false;
		OnItemChosen.Broadcast(Item);
		return true;
	}
	return Item.IsSet() && BeginHold(Item);
}

EInventoryDropAction UInventoryUISubsystem::PreviewPlacement(FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
{
	return ResolvePlacement(Container, Placement, false);
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

	FInventoryPlacement Target = Placement;
	Target.bRotated = bHeldRotated;
	const int32 MoveCount = HeldCount < Held->Count ? HeldCount : 0;
	const FInventoryItemHandle Occupant = Registry->GetItemAt(Container, Target);
	const bool bOccupiedByOther = Occupant.IsSet() && Occupant != HeldItem;
	const bool bMerge = bOccupiedByOther && Registry->CanStack(HeldItem, Occupant);

	// Equipment slots make way like the equipment component does: what's in the way goes to the owner's storage.
	const UInventoryContainerLayout& TargetLayout = *TargetRecord->Layout;
	const bool bEquipmentSlot = TargetLayout.bEquipment && TargetLayout.Slots.IsValidIndex(Target.Index);
	const FGameplayTag EquipSlot = bEquipmentSlot ? TargetLayout.Slots[Target.Index].Tag : FGameplayTag();
	const UInventoryComponent* SlotOwner = bEquipmentSlot ? Cast<UInventoryComponent>(Registry->GetOwner(Container)) : nullptr;
	const TArray<FInventoryContainerHandle> Storage = SlotOwner ? SlotOwner->GetStorageContainers() : TArray<FInventoryContainerHandle>();

	auto MoveHeld = [&]() { return Registry->MoveItem(HeldItem, Container, Target, MoveCount); };
	auto SwapHeld = [&]() { return Registry->SwapItems(HeldItem, Occupant); };
	auto EquipHeld = [&]() { return Registry->EquipItem(HeldItem, Container, EquipSlot, Storage); };
	auto Attempt = [&](TFunctionRef<EInventoryError()> Operation) { return bApply ? Operation() : Registry->DryRun(Operation); };

	if (Attempt(MoveHeld) == EInventoryError::None)
	{
		return bMerge ? EInventoryDropAction::Merge : EInventoryDropAction::Place;
	}

	// Only whole stacks swap: the rest of a split stack would have nowhere to go.
	if (bOccupiedByOther && !bMerge && MoveCount == 0 && Attempt(SwapHeld) == EInventoryError::None)
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
}
