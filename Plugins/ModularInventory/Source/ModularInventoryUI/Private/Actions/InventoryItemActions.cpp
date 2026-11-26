#include "Actions/InventoryItemActions.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryCapacityUpgrade.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"
#include "Data/InventoryItemUse.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventoryComponent.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Shortcuts/InventoryShortcutComponent.h"
#include "Widgets/InventoryRecipeMenuWidget.h"
#include "Widgets/InventoryScreenWidget.h"
#include "Widgets/InventoryShortcutMenuWidget.h"

#define LOCTEXT_NAMESPACE "InventoryItemActions"

namespace
{
	/** The Inventory Component of the pawn the action's player controls. */
	UInventoryComponent* FindPlayerInventory(const FInventoryItemActionContext& Context)
	{
		return Context.Player ? UInventoryComponent::FindInventoryComponent(Context.Player->GetPawn()) : nullptr;
	}

	UInventoryEquipmentComponent* FindPlayerEquipment(const FInventoryItemActionContext& Context)
	{
		return Context.Player ? UInventoryEquipmentComponent::FindEquipmentComponent(Context.Player->GetPawn()) : nullptr;
	}

	const UInventoryItemDefinition* FindDefinition(const UObject* ContextObject, FInventoryItemHandle Item)
	{
		const UInventorySubsystem* Inventory = UInventorySubsystem::Get(ContextObject);
		return Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
	}
}

// Move
UInventoryItemAction_Move::UInventoryItemAction_Move()
{
	DisplayName = LOCTEXT("MoveName", "Move");
	Description = LOCTEXT("MoveDescription", "Pick the item up to put it somewhere else.");
}

bool UInventoryItemAction_Move::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryUISubsystem* Interaction = GetInteraction(Context);
	return Interaction && Interaction->CanBeginHold(Context.Item);
}

void UInventoryItemAction_Move::Run_Implementation(const FInventoryItemActionContext& Context)
{
	// As the Move action does: held by the cell aimed at when that's one of its own, or else by its middle cell, where it lies.
	if (UInventoryUISubsystem* Interaction = GetInteraction(Context))
	{
		Interaction->SetHoveredItem(Context.Item);
		Interaction->PickUpOrPlace();
	}
}

// Split
UInventoryItemAction_Split::UInventoryItemAction_Split()
{
	DisplayName = LOCTEXT("SplitName", "Split");
	Description = LOCTEXT("SplitDescription", "Take part of the stack to put somewhere else.");
	Prompt = EInventoryActionPrompt::Quantity;
	PromptText = LOCTEXT("SplitPrompt", "Take how many?");
}

bool UInventoryItemAction_Split::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryUISubsystem* Interaction = GetInteraction(Context);
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Context.Item) : nullptr;
	return Interaction && Definition && Definition->CanSplit() && Inventory->GetItemCount(Context.Item) > 1 && Interaction->CanBeginHold(Context.Item);
}

void UInventoryItemAction_Split::GetQuantityRange_Implementation(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const
{
	// All but one, as the rest stays where it is.
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const int32 Count = Inventory ? Inventory->GetItemCount(Context.Item) : 2;
	OutMin = 1;
	OutMax = FMath::Max(1, Count - 1);
	OutDefault = FMath::Max(1, Count / 2);
}

void UInventoryItemAction_Split::Run_Implementation(const FInventoryItemActionContext& Context)
{
	// Picked up as Move does, so it's held by the same cell, then cut down to the count chosen.
	UInventoryUISubsystem* Interaction = GetInteraction(Context);
	if (!Interaction)
	{
		return;
	}
	Interaction->SetHoveredItem(Context.Item);
	if (Interaction->PickUpOrPlace() && Context.Count > 0)
	{
		Interaction->SetHeldCount(Context.Count);
	}
}

// Transfer
UInventoryItemAction_Transfer::UInventoryItemAction_Transfer()
{
	DisplayName = LOCTEXT("TransferName", "Transfer");
	Description = LOCTEXT("TransferDescription", "Move the item to the other side.");
}

bool UInventoryItemAction_Transfer::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryUISubsystem* Interaction = GetInteraction(Context);
	return Interaction && Interaction->CanTransferItem(Context.Item);
}

void UInventoryItemAction_Transfer::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UInventoryUISubsystem* Interaction = GetInteraction(Context))
	{
		Interaction->TransferItem(Context.Item);
	}
}

// Apply Capacity Upgrade
UInventoryItemAction_ApplyCapacityUpgrade::UInventoryItemAction_ApplyCapacityUpgrade()
{
	DisplayName = LOCTEXT("ApplyUpgradeName", "Use");
	Description = LOCTEXT("ApplyUpgradeDescription", "Use it to make room for more.");
	AtMaxCapacityReason = LOCTEXT("ApplyUpgradeAtMax", "There's no more room to add.");
}

bool UInventoryItemAction_ApplyCapacityUpgrade::AppliesTo(const FInventoryItemActionContext& Context) const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Context.Item) : nullptr;
	return Definition && Definition->FindFragment<FInventoryCapacityUpgrade>() && Super::AppliesTo(Context);
}

bool UInventoryItemAction_ApplyCapacityUpgrade::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryComponent* Inventory = FindPlayerInventory(Context);
	return Inventory && Inventory->CanApplyCapacityUpgrade(Context.Item);
}

FText UInventoryItemAction_ApplyCapacityUpgrade::GetUnavailableReason_Implementation(const FInventoryItemActionContext& Context) const
{
	// Applies To already made sure it's an upgrade. Only a container the player has that can't grow is explained; a player without the
	// container, or without an Inventory Component, isn't who the upgrade is for.
	const UInventoryComponent* Inventory = FindPlayerInventory(Context);
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(GetContextObject(Context));
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Context.Item) : nullptr;
	const FInventoryCapacityUpgrade* Upgrade = Definition ? Definition->FindFragment<FInventoryCapacityUpgrade>() : nullptr;
	const bool bHasContainer = Inventory && Upgrade && Inventory->GetContainer(Upgrade->ContainerTag).IsSet();
	return bHasContainer ? AtMaxCapacityReason : FText::GetEmpty();
}

void UInventoryItemAction_ApplyCapacityUpgrade::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UInventoryComponent* Inventory = FindPlayerInventory(Context))
	{
		EInventoryError Error = EInventoryError::None;
		Inventory->ApplyCapacityUpgrade(Context.Item, Error);
	}
}

// Use
UInventoryItemAction_Use::UInventoryItemAction_Use()
{
	DisplayName = LOCTEXT("UseName", "Use");
	Description = LOCTEXT("UseDescription", "Use the item.");
}

bool UInventoryItemAction_Use::AppliesTo(const FInventoryItemActionContext& Context) const
{
	const UInventoryItemDefinition* Definition = FindDefinition(GetContextObject(Context), Context.Item);
	return Super::AppliesTo(Context) && Definition && Definition->FindFragment<FInventoryItemUse>();
}

bool UInventoryItemAction_Use::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryComponent* Inventory = FindPlayerInventory(Context);
	return Inventory && Inventory->CanUseItem(Context.Item);
}

void UInventoryItemAction_Use::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UInventoryComponent* Inventory = FindPlayerInventory(Context))
	{
		Inventory->UseItem(Context.Item);
	}
}

// Drop
UInventoryItemAction_Drop::UInventoryItemAction_Drop()
{
	DisplayName = LOCTEXT("DropName", "Drop");
	Description = LOCTEXT("DropDescription", "Drop the item here.");
	Prompt = EInventoryActionPrompt::Confirm;
	PromptText = LOCTEXT("DropPrompt", "Drop {Item}?");
}

bool UInventoryItemAction_Drop::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryComponent* Inventory = FindPlayerInventory(Context);
	return Inventory && Inventory->CanDropItem(Context.Item);
}

void UInventoryItemAction_Drop::Run_Implementation(const FInventoryItemActionContext& Context)
{
	// A count only when the prompt asked how many; otherwise the whole stack.
	if (UInventoryComponent* Inventory = FindPlayerInventory(Context))
	{
		Inventory->DropItem(Context.Item, Prompt == EInventoryActionPrompt::Quantity ? Context.Count : 0);
	}
}

// Equip
UInventoryItemAction_Equip::UInventoryItemAction_Equip()
{
	DisplayName = LOCTEXT("EquipName", "Equip");
	Description = LOCTEXT("EquipDescription", "Equip the item.");
	bKeepsMenuOpen = true;
}

bool UInventoryItemAction_Equip::AppliesTo(const FInventoryItemActionContext& Context) const
{
	const UInventoryItemDefinition* Definition = FindDefinition(GetContextObject(Context), Context.Item);
	return Super::AppliesTo(Context) && Definition && Definition->EquipRules;
}

FGameplayTag UInventoryItemAction_Equip::ChooseSlot(const FInventoryItemActionContext& Context) const
{
	const UInventoryEquipmentComponent* Equipment = FindPlayerEquipment(Context);
	return Slot.IsValid() || !Equipment ? Slot : Equipment->FindSlotFor(Context.Item);
}

bool UInventoryItemAction_Equip::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryEquipmentComponent* Equipment = FindPlayerEquipment(Context);
	const UInventoryComponent* Inventory = FindPlayerInventory(Context);
	const UInventoryItemDefinition* Definition = FindDefinition(GetContextObject(Context), Context.Item);
	const UInventoryEquipRules* Rules = Definition ? Definition->EquipRules.Get() : nullptr;
	const FGameplayTag ChosenSlot = ChooseSlot(Context);
	return Equipment && Inventory && Rules && Inventory->OwnsItem(Context.Item) && !Equipment->IsEquipped(Context.Item) && ChosenSlot.IsValid() && Rules->AllowsSlot(ChosenSlot);
}

void UInventoryItemAction_Equip::Run_Implementation(const FInventoryItemActionContext& Context)
{
	UInventoryEquipmentComponent* Equipment = FindPlayerEquipment(Context);
	const FGameplayTag ChosenSlot = ChooseSlot(Context);
	EInventoryError Error = EInventoryError::None;
	if (Equipment && bEquipInPlace)
	{
		Equipment->EquipInPlace(Context.Item, ChosenSlot, Error);
	}
	else if (Equipment)
	{
		Equipment->EquipItem(Context.Item, ChosenSlot, Error);
	}
}

// Unequip
UInventoryItemAction_Unequip::UInventoryItemAction_Unequip()
{
	DisplayName = LOCTEXT("UnequipName", "Unequip");
	Description = LOCTEXT("UnequipDescription", "Take the item off.");
	bKeepsMenuOpen = true;
}

bool UInventoryItemAction_Unequip::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryEquipmentComponent* Equipment = FindPlayerEquipment(Context);
	return Equipment && Equipment->IsEquipped(Context.Item);
}

void UInventoryItemAction_Unequip::Run_Implementation(const FInventoryItemActionContext& Context)
{
	EInventoryError Error = EInventoryError::None;
	if (UInventoryEquipmentComponent* Equipment = FindPlayerEquipment(Context))
	{
		Equipment->UnequipItem(Context.Item, Error);
	}
}

// Assign Shortcut
UInventoryItemAction_AssignShortcut::UInventoryItemAction_AssignShortcut()
{
	DisplayName = LOCTEXT("AssignShortcutName", "Shortcut");
	Description = LOCTEXT("AssignShortcutDescription", "Put the item on a shortcut.");
}

bool UInventoryItemAction_AssignShortcut::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryShortcutMenuWidget* MenuDefaults = MenuClass ? GetDefault<UInventoryShortcutMenuWidget>(MenuClass) : nullptr;
	return MenuDefaults && Context.Screen && MenuDefaults->CanOpenFor(Context);
}

void UInventoryItemAction_AssignShortcut::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (Context.Screen)
	{
		Context.Screen->OpenItemMenu(Context.Item, MenuClass);
	}
}

// Craft
UInventoryItemAction_Craft::UInventoryItemAction_Craft()
{
	DisplayName = LOCTEXT("CraftName", "Craft");
	Description = LOCTEXT("CraftDescription", "Make something from this item.");
}

const UInventoryRecipeBook* UInventoryItemAction_Craft::GetRecipeBook(const FInventoryItemActionContext& Context) const
{
	if (RecipeBook)
	{
		return RecipeBook;
	}
	const UInventoryRecipeMenuWidget* MenuDefaults = MenuClass ? GetDefault<UInventoryRecipeMenuWidget>(MenuClass) : nullptr;
	return MenuDefaults ? MenuDefaults->GetDefaultRecipeBook() : nullptr;
}

bool UInventoryItemAction_Craft::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryComponent* Crafter = FindPlayerInventory(Context);
	const UInventoryRecipeBook* Book = GetRecipeBook(Context);
	if (!MenuClass || !Context.Screen || !Crafter || !Book)
	{
		return false;
	}

	const TArray<UInventoryRecipe*> Recipes = Crafter->GetRecipesUsing(Book, Context.Item);
	if (!bOnlyWhenCraftable)
	{
		return !Recipes.IsEmpty();
	}
	return Recipes.ContainsByPredicate([this, Crafter](const UInventoryRecipe* Recipe)
	{
		return Crafter->GetCraftError(Recipe, ContextTags, 1) == EInventoryError::None;
	});
}

void UInventoryItemAction_Craft::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (!Context.Screen)
	{
		return;
	}

	// The menu is given its recipes before its item, so it lists them as it's measured and placed.
	const UInventoryRecipeBook* Book = GetRecipeBook(Context);
	const FGameplayTagContainer& Tags = ContextTags;
	Context.Screen->OpenItemMenuWith(Context.Item, MenuClass, [Book, &Tags](UInventoryItemMenuWidget& Menu)
	{
		if (UInventoryRecipeMenuWidget* RecipeMenu = Cast<UInventoryRecipeMenuWidget>(&Menu))
		{
			RecipeMenu->SetRecipeSource(Book, Tags);
		}
	});
}

#undef LOCTEXT_NAMESPACE
