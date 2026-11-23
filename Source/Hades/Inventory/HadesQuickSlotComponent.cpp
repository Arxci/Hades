#include "HadesQuickSlotComponent.h"

#include "Hades/Character/Hero/HadesHeroCharacter.h"
#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Input/HadesInputConfig.h"
#include "Hades/Inventory/HadesInventoryComponent.h"

#include "Equipment/InventoryEquipmentComponent.h"

UHadesQuickSlotComponent::UHadesQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SlotInputs = { HadesGameplayTags::Input_QuickSlot_1, HadesGameplayTags::Input_QuickSlot_2, HadesGameplayTags::Input_QuickSlot_3, HadesGameplayTags::Input_QuickSlot_4 };
}

void UHadesQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(GetNumSlots());
	Inventory = Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwner()));
	if (!Inventory)
	{
		UE_LOG(LogHadesInventory, Warning, TEXT("[Quick Slots] %s needs a Hades Inventory Component on %s."), *GetNameSafe(this), *GetNameSafe(GetOwner()));
		return;
	}
	Inventory->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);

	// What's in hand changes without the inventory changing, since it stays in the inventory.
	Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwner());
	if (Equipment)
	{
		Equipment->OnItemEquipped.AddDynamic(this, &ThisClass::HandleEquipmentChanged);
		Equipment->OnItemUnequipped.AddDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
}

void UHadesQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	if (Equipment)
	{
		Equipment->OnItemEquipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Equipment->OnItemUnequipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
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
	if (Slots.IsValidIndex(SlotIndex))
	{
		OnSlotActivated.Broadcast(SlotIndex);
	}

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

int32 UHadesQuickSlotComponent::FindSlot(FInventoryItemHandle Item) const
{
	return Item.IsSet() ? Slots.IndexOfByKey(Item) : INDEX_NONE;
}

FGameplayTag UHadesQuickSlotComponent::GetSlotInputTag(int32 SlotIndex) const
{
	return SlotInputs.IsValidIndex(SlotIndex) ? SlotInputs[SlotIndex] : FGameplayTag();
}

const UInputAction* UHadesQuickSlotComponent::GetSlotInputAction(int32 SlotIndex) const
{
	// The hero's input config is where tags meet input actions, so it's asked rather than keeping a copy here.
	const AHadesHeroCharacter* Hero = Cast<AHadesHeroCharacter>(GetOwner());
	const UHadesInputConfig* InputConfig = Hero ? Hero->GetInputConfig() : nullptr;
	const FGameplayTag InputTag = GetSlotInputTag(SlotIndex);
	return InputConfig && InputTag.IsValid() ? InputConfig->FindInputActionForTag(InputTag, false) : nullptr;
}

bool UHadesQuickSlotComponent::CanAssign(FInventoryItemHandle Item) const
{
	return Inventory && (Inventory->CanDraw(Item) || Inventory->CanUseItem(Item));
}

void UHadesQuickSlotComponent::HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag Slot)
{
	OnChanged.Broadcast();
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
