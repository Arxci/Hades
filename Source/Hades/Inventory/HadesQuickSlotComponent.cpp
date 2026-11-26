#include "HadesQuickSlotComponent.h"

#include "Hades/Character/Hero/HadesHeroCharacter.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Input/HadesInputConfig.h"
#include "Hades/Inventory/HadesInventoryComponent.h"

#include "Equipment/InventoryEquipmentComponent.h"

#define LOCTEXT_NAMESPACE "HadesQuickSlots"

UHadesQuickSlotComponent::UHadesQuickSlotComponent()
{
	// A slot per quick slot key, each told apart by the input tag of its key.
	const FGameplayTag SlotTags[] = { HadesGameplayTags::Input_QuickSlot_1, HadesGameplayTags::Input_QuickSlot_2, HadesGameplayTags::Input_QuickSlot_3, HadesGameplayTags::Input_QuickSlot_4 };
	Rules.Slots.Reset();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotTags); ++Index)
	{
		FInventoryShortcutSlotDefinition& Slot = Rules.Slots.AddDefaulted_GetRef();
		Slot.Tag = SlotTags[Index];
		Slot.DisplayName = FText::AsNumber(Index + 1);
	}

	// A slot empties when its item leaves, rather than moving on to another of the same kind.
	Rules.bFollowSameKind = false;
}

void UHadesQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwner());
	if (Equipment)
	{
		Equipment->OnItemEquipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
		Equipment->OnItemUnequipped.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
}

void UHadesQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Equipment)
	{
		Equipment->OnItemEquipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
		Equipment->OnItemUnequipped.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	Equipment = nullptr;

	Super::EndPlay(EndPlayReason);
}

UHadesInventoryComponent* UHadesQuickSlotComponent::GetHadesInventory() const
{
	return Cast<UHadesInventoryComponent>(GetInventory());
}

bool UHadesQuickSlotComponent::AcceptsItem_Implementation(int32 SlotIndex, FInventoryItemHandle Item) const
{
	// Something to hold or to use.
	const UHadesInventoryComponent* HadesInventory = GetHadesInventory();
	return HadesInventory && (HadesInventory->CanDraw(Item) || HadesInventory->CanUseItem(Item));
}

void UHadesQuickSlotComponent::NativeOnShortcutActivated(int32 SlotIndex, FInventoryItemHandle Item)
{
	UHadesInventoryComponent* HadesInventory = GetHadesInventory();
	if (!HadesInventory || !Item.IsSet())
	{
		return;
	}

	if (HadesInventory->IsInHand(Item))
	{
		HadesInventory->StowItem(Item);
	}
	else if (HadesInventory->CanDraw(Item))
	{
		HadesInventory->DrawItem(Item);
	}
	else
	{
		HadesInventory->UseItem(Item);
	}
}

const UInputAction* UHadesQuickSlotComponent::GetSlotInputAction_Implementation(int32 SlotIndex) const
{
	// The hero's input config is where tags meet input actions, so it's asked rather than keeping a copy on each slot.
	const AHadesHeroCharacter* Hero = Cast<AHadesHeroCharacter>(GetOwner());
	const UHadesInputConfig* InputConfig = Hero ? Hero->GetInputConfig() : nullptr;
	const FGameplayTag InputTag = GetSlotTag(SlotIndex);
	if (InputConfig && InputTag.IsValid())
	{
		if (const UInputAction* Found = InputConfig->FindInputActionForTag(InputTag, false))
		{
			return Found;
		}
	}
	return Super::GetSlotInputAction_Implementation(SlotIndex);
}

void UHadesQuickSlotComponent::HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag Slot)
{
	OnShortcutsChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
