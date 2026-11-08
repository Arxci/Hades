#include "HadesQuickSlotComponent.h"

#include "Hades/Hades.h"
#include "Hades/Inventory/HadesInventoryComponent.h"

UHadesQuickSlotComponent::UHadesQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHadesQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(NumSlots);
	Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwner()));
	if (!Inventory)
	{
		UE_LOG(LogHadesInventory, Warning, TEXT("[Quick Slots] %s needs a Hades Inventory Component on %s."), *GetNameSafe(this), *GetNameSafe(GetOwner()));
		return;
	}
	Inventory->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);
}

void UHadesQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UHadesQuickSlotComponent::AssignItem(int32 SlotIndex, FInventoryItemHandle Item)
{
	if (!Slots.IsValidIndex(SlotIndex) || !CanAssign(Item))
	{
		return;
	}

	for (FInventoryItemHandle& Assigned : Slots)
	{
		if (Assigned == Item)
		{
			Assigned.Reset();
		}
	}
	Slots[SlotIndex] = Item;
	OnChanged.Broadcast();
}

void UHadesQuickSlotComponent::AssignToFreeSlot(FInventoryItemHandle Item)
{
	const int32 FreeSlot = Slots.IndexOfByPredicate([](const FInventoryItemHandle& Assigned) { return !Assigned.IsSet(); });
	if (FreeSlot != INDEX_NONE && !Slots.Contains(Item) && Inventory && Inventory->CanDraw(Item))
	{
		AssignItem(FreeSlot, Item);
	}
}

void UHadesQuickSlotComponent::ClearSlot(int32 SlotIndex)
{
	if (Slots.IsValidIndex(SlotIndex) && Slots[SlotIndex].IsSet())
	{
		Slots[SlotIndex].Reset();
		OnChanged.Broadcast();
	}
}

void UHadesQuickSlotComponent::ActivateSlot(int32 SlotIndex)
{
	const FInventoryItemHandle Item = GetSlotItem(SlotIndex);
	if (!Inventory || !Item.IsSet())
	{
		return;
	}

	if (Inventory->IsInHand(Item))
	{
		Inventory->StowItem(Item);
	}
	else if (Inventory->CanDraw(Item))
	{
		Inventory->DrawItem(Item);
	}
	else
	{
		Inventory->UseItem(Item);
	}
}

FInventoryItemHandle UHadesQuickSlotComponent::GetSlotItem(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FInventoryItemHandle();
}

bool UHadesQuickSlotComponent::CanAssign(FInventoryItemHandle Item) const
{
	return Inventory && (Inventory->CanDraw(Item) || Inventory->CanUseItem(Item));
}

void UHadesQuickSlotComponent::HandleInventoryChanged(UInventoryComponent* ChangedInventory)
{
	for (FInventoryItemHandle& Item : Slots)
	{
		if (Item.IsSet() && !Inventory->OwnsItem(Item))
		{
			Item.Reset();
		}
	}
	OnChanged.Broadcast();
}
