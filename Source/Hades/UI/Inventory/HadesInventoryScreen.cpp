#include "HadesInventoryScreen.h"

#include "GameFramework/Pawn.h"
#include "Input/CommonUIInputTypes.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Player/HadesPlayerController.h"

#include "InventoryUISubsystem.h"
#include "Widgets/InventoryListWidget.h"

#define LOCTEXT_NAMESPACE "HadesInventoryScreen"

UHadesInventoryScreen::UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// A left click does nothing, so items are only moved on purpose, by a drag or Move.
	InteractionStyle.LeftClick = EInventoryItemPress::None;
	InteractionStyle.RightClick = EInventoryItemPress::OpenItemMenu;
	InteractionStyle.Accept = EInventoryItemPress::OpenItemMenu;
	InteractionStyle.bSnapHeldItem = true;
	InteractionStyle.bSwapPicksUpOther = true;
	InteractionStyle.bKeepLastHoveredItem = true;

	// Opens on the item last highlighted, or the inventory's first cell, highlighted and in the details, with the mouse too.
	bAimOnOpen = true;
	bStartOnLastItem = true;
	bPauseGame = true;

	// Item menus open on the Modal layer, above the screen, so the screen stays drawn behind them.
	ItemMenuLayer = HadesGameplayTags::UI_Layer_Modal;

	// The items are the second page, after the map, and the tab keys turn a held item.
	ItemsPage = 1;
	bTabKeysTurnHeldItem = true;

	// The line along the bottom of the screen: what to do with a held item, or which item to choose. An item menu says what its highlighted
	// option does there. Browsing the items, it's empty: the details panel shows what the hovered item is.
	MoveHint = LOCTEXT("MoveHint", "Select an area to place this item.");
	ChooseHint = LOCTEXT("CombineHint", "Select an item to combine with.");
	InvalidHint = LOCTEXT("InvalidHint", "That can't go there.");
	InvalidHints.Add(EInventoryError::Occupied, LOCTEXT("OccupiedHint", "Something's in the way."));
	InvalidHints.Add(EInventoryError::OutOfBounds, LOCTEXT("OutOfBoundsHint", "That space can't be used yet."));
	InvalidHints.Add(EInventoryError::CannotStack, LOCTEXT("CannotStackHint", "That stack is full."));
	InvalidHints.Add(EInventoryError::NoSpace, LOCTEXT("NoSpaceHint", "Not enough space."));
}

void UHadesInventoryScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The key that opens the inventory closes it too.
	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Inventory), false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseAction)));
}

void UHadesInventoryScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemChosen.AddUniqueDynamic(this, &ThisClass::HandleItemChosen);
	}
	if (ItemBoxList)
	{
		ItemBoxList->SetContainer(GetItemBox());
	}
}

void UHadesInventoryScreen::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemChosen.RemoveDynamic(this, &ThisClass::HandleItemChosen);
	}

	Super::NativeOnDeactivated();
}

void UHadesInventoryScreen::HandleItemChosen(FInventoryItemHandle Item, FInventoryItemHandle Chosen)
{
	// Only the Combine action has the player choose an item.
	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	if (Inventory && Item.IsSet())
	{
		Inventory->CombineItems(Item, Chosen);
	}
}

void UHadesInventoryScreen::NativeOnSorted(bool bSorted)
{
	Super::NativeOnSorted(bSorted);

	if (!bSorted)
	{
		AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("CantOrganize", "Couldn't organize your inventory."));
	}
}

void UHadesInventoryScreen::HandleCloseAction()
{
	CloseWidget();
}

// Helpers
UHadesInventoryComponent* UHadesInventoryScreen::GetPlayerInventory() const
{
	return Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetOwningPlayerPawn()));
}

FInventoryContainerHandle UHadesInventoryScreen::GetItemBox() const
{
	const AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer());
	const UInventoryComponent* Box = ItemBoxList && Controller ? Controller->GetItemBoxInventory() : nullptr;
	return Box ? Box->GetContainer(HadesGameplayTags::Container_ItemBox) : FInventoryContainerHandle();
}

#undef LOCTEXT_NAMESPACE
