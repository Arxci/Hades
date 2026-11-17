#include "HadesItemActions.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/Player/HadesPlayerController.h"
#include "Hades/UI/Inventory/HadesInventoryScreen.h"
#include "Hades/UI/Inventory/HadesQuickSlotMenu.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"

#define LOCTEXT_NAMESPACE "HadesItemActions"

namespace
{
	FInventoryContainerHandle GetItemBox(const FInventoryItemActionContext& Context)
	{
		const UHadesInventoryScreen* Screen = Cast<UHadesInventoryScreen>(Context.Screen);
		return Screen ? Screen->GetItemBox() : FInventoryContainerHandle();
	}

	void ShowMessage(const FInventoryItemActionContext& Context, const FText& Message)
	{
		if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(Context.Player))
		{
			Controller->ShowMessage(Message);
		}
	}
}

UHadesInventoryComponent* UHadesItemAction::GetPlayerInventory(const FInventoryItemActionContext& Context)
{
	const APawn* Pawn = Context.Player ? Context.Player->GetPawn() : nullptr;
	return Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(Pawn));
}

// Use
UHadesItemAction_Use::UHadesItemAction_Use()
{
	DisplayName = LOCTEXT("UseName", "Use");
	Description = LOCTEXT("UseDescription", "Use the item.");
}

bool UHadesItemAction_Use::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	return Inventory && Inventory->CanUseItem(Context.Item);
}

void UHadesItemAction_Use::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UHadesInventoryComponent* Inventory = GetPlayerInventory(Context))
	{
		Inventory->UseItem(Context.Item);
	}
}

// Equip
UHadesItemAction_Equip::UHadesItemAction_Equip()
{
	DisplayName = LOCTEXT("EquipName", "Equip");
	Description = LOCTEXT("EquipDescription", "Hold the item in your hands.");
}

bool UHadesItemAction_Equip::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	return Inventory && Inventory->CanDraw(Context.Item) && !Inventory->IsInHand(Context.Item);
}

void UHadesItemAction_Equip::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UHadesInventoryComponent* Inventory = GetPlayerInventory(Context))
	{
		Inventory->DrawItem(Context.Item);
	}
}

// Unequip
UHadesItemAction_Unequip::UHadesItemAction_Unequip()
{
	DisplayName = LOCTEXT("UnequipName", "Unequip");
	Description = LOCTEXT("UnequipDescription", "Put the item away.");
}

bool UHadesItemAction_Unequip::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	return Inventory && Inventory->IsInHand(Context.Item);
}

void UHadesItemAction_Unequip::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UHadesInventoryComponent* Inventory = GetPlayerInventory(Context))
	{
		Inventory->StowItem(Context.Item);
	}
}

// Combine
UHadesItemAction_Combine::UHadesItemAction_Combine()
{
	DisplayName = LOCTEXT("CombineName", "Combine");
	Description = LOCTEXT("CombineDescription", "Combine the item with another one.");
}

bool UHadesItemAction_Combine::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	return Inventory && GetInteraction(Context) && Inventory->CanCombine(Context.Item);
}

void UHadesItemAction_Combine::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UInventoryUISubsystem* Interaction = GetInteraction(Context))
	{
		Interaction->BeginChoosingItem(Context.Item);
	}
}

// Discard
UHadesItemAction_Discard::UHadesItemAction_Discard()
{
	DisplayName = LOCTEXT("DiscardName", "Discard");
	Description = LOCTEXT("DiscardDescription", "Drop the item here.");
	Prompt = EInventoryActionPrompt::Confirm;
	PromptText = LOCTEXT("DiscardPrompt", "Discard {Item}?");
}

bool UHadesItemAction_Discard::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	return Inventory && Inventory->CanDiscardItem(Context.Item);
}

void UHadesItemAction_Discard::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UHadesInventoryComponent* Inventory = GetPlayerInventory(Context))
	{
		Inventory->DiscardItem(Context.Item);
	}
}

// Store
UHadesItemAction_Store::UHadesItemAction_Store()
{
	DisplayName = LOCTEXT("StoreName", "Store");
	Description = LOCTEXT("StoreDescription", "Put the item in the item box.");
}

bool UHadesItemAction_Store::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	return Inventory && GetItemBox(Context).IsSet() && Inventory->OwnsItem(Context.Item) && !Inventory->IsKeyItem(Context.Item);
}

void UHadesItemAction_Store::Run_Implementation(const FInventoryItemActionContext& Context)
{
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(GetContextObject(Context));
	EInventoryError Error = EInventoryError::None;
	if (Subsystem && !Subsystem->MoveItemToContainers(Context.Item, { GetItemBox(Context) }, 0, false, Error))
	{
		ShowMessage(Context, LOCTEXT("CantStore", "That can't go in the item box."));
	}
}

// Take
UHadesItemAction_Take::UHadesItemAction_Take()
{
	DisplayName = LOCTEXT("TakeName", "Take");
	Description = LOCTEXT("TakeDescription", "Take the item from the item box.");
}

bool UHadesItemAction_Take::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(GetContextObject(Context));
	const FInventoryContainerHandle ItemBox = GetItemBox(Context);
	return GetPlayerInventory(Context) && Subsystem && ItemBox.IsSet() && Subsystem->GetRootContainer(Subsystem->GetItemContainer(Context.Item)) == ItemBox;
}

void UHadesItemAction_Take::Run_Implementation(const FInventoryItemActionContext& Context)
{
	UHadesInventoryComponent* Inventory = GetPlayerInventory(Context);
	EInventoryError Error = EInventoryError::None;
	if (Inventory && !Inventory->TakeItem(Context.Item, 0, true, Error))
	{
		ShowMessage(Context, LOCTEXT("NoRoomToTake", "Not enough space."));
	}
}

// Assign Quick Slot
UHadesItemAction_AssignQuickSlot::UHadesItemAction_AssignQuickSlot()
{
	DisplayName = LOCTEXT("AssignQuickSlotName", "Quick Slot");
	Description = LOCTEXT("AssignQuickSlotDescription", "Put the item on a quick slot key.");
}

bool UHadesItemAction_AssignQuickSlot::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const APawn* Pawn = Context.Player ? Context.Player->GetPawn() : nullptr;
	const UHadesQuickSlotComponent* QuickSlots = Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
	return MenuClass && Context.Screen && QuickSlots && QuickSlots->CanAssign(Context.Item);
}

void UHadesItemAction_AssignQuickSlot::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (Context.Screen)
	{
		Context.Screen->OpenItemMenu(Context.Item, MenuClass);
	}
}

#undef LOCTEXT_NAMESPACE
