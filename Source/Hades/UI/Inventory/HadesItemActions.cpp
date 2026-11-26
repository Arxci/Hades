#include "HadesItemActions.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Player/HadesPlayerController.h"
#include "Hades/UI/Inventory/HadesInventoryScreen.h"

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

	UHadesInventoryComponent* FindHadesInventory(const FInventoryItemActionContext& Context)
	{
		const APawn* Pawn = Context.Player ? Context.Player->GetPawn() : nullptr;
		return Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(Pawn));
	}
}

UHadesInventoryComponent* UHadesItemAction::GetPlayerInventory(const FInventoryItemActionContext& Context)
{
	return FindHadesInventory(Context);
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
	Description = LOCTEXT("CombineDescription", "Combine items to make something new.");
}

const UInventoryRecipeBook* UHadesItemAction_Combine::GetRecipeBook(const FInventoryItemActionContext& Context) const
{
	// The player's own Combine Recipes, which choosing the other item combines from too, so both ways make the same things.
	const UHadesInventoryComponent* Inventory = FindHadesInventory(Context);
	const UInventoryRecipeBook* Book = Inventory ? Inventory->GetCombineRecipes() : nullptr;
	return Book ? Book : Super::GetRecipeBook(Context);
}

bool UHadesItemAction_Combine::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	// The recipe list for anything a recipe takes, with Recipe List and a Menu Class; choosing the other item for everything else, such as a
	// gun and its ammo.
	const UHadesInventoryComponent* Inventory = FindHadesInventory(Context);
	const bool bCanChoose = Inventory && GetInteraction(Context) && Inventory->CanCombine(Context.Item);
	return bCanChoose || (Style == EHadesCombineStyle::RecipeList && Super::CanRun_Implementation(Context));
}

void UHadesItemAction_Combine::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (Style == EHadesCombineStyle::RecipeList && Super::CanRun_Implementation(Context))
	{
		Super::Run_Implementation(Context);
	}
	else if (UInventoryUISubsystem* Interaction = GetInteraction(Context))
	{
		Interaction->BeginChoosingItem(Context.Item);
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

#undef LOCTEXT_NAMESPACE
