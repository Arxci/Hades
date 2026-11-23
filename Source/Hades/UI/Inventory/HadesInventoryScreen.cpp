#include "HadesInventoryScreen.h"

#include "CommonAnimatedSwitcher.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "Input/CommonUIInputTypes.h"
#include "PrimaryGameLayout.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Player/HadesPlayerController.h"
#include "Hades/UI/Foundation/HadesTabListWidget.h"

#include "InventoryUISubsystem.h"
#include "Widgets/InventoryItemMenuWidget.h"
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

	RegisterUIActionBinding(FBindUIActionArgs(FUIActionTag::ConvertChecked(HadesGameplayTags::UI_Action_Inventory), false,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCloseAction)));

	if (TabList)
	{
		TabList->OnTabSelected.AddUniqueDynamic(this, &ThisClass::HandleTabSelected);
	}
}

void UHadesInventoryScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// The tab list is built by now, so it makes its tabs as it's linked.
	if (TabList && Pages && TabList->GetLinkedSwitcher() != Pages)
	{
		TabList->SetLinkedSwitcher(Pages);
	}
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

	const bool bItemsTabOpened = TabList && TabList->OpenTabAtIndex(ItemsPage, /*bSuppressClickFeedback*/ true);
	if (!bItemsTabOpened && Pages)
	{
		Pages->SetActiveWidgetIndex(ItemsPage);
	}
	SetInventoryShown(IsItemsPageOpen());
}

void UHadesInventoryScreen::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemChosen.RemoveDynamic(this, &ThisClass::HandleItemChosen);
	}

	Super::NativeOnDeactivated();
}

UInventoryItemMenuWidget* UHadesInventoryScreen::PushItemMenu(TSubclassOf<UInventoryItemMenuWidget> MenuClass, TFunctionRef<void(UInventoryItemMenuWidget&)> InitMenu)
{
	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	return Layout ? Layout->PushWidgetToLayerStack<UInventoryItemMenuWidget>(HadesGameplayTags::UI_Layer_Modal, MenuClass.Get(), InitMenu) : nullptr;
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

// Pages
bool UHadesInventoryScreen::IsItemsPageOpen() const
{
	const int32 OpenTab = TabList ? TabList->GetOpenTabIndex() : INDEX_NONE;
	if (OpenTab != INDEX_NONE)
	{
		return OpenTab == ItemsPage;
	}
	return !Pages || Pages->GetActiveWidgetIndex() == ItemsPage;
}

void UHadesInventoryScreen::HandleTabSelected(FName TabId)
{
	SetInventoryShown(TabList->GetTabIndexOf(TabId) == ItemsPage);
}

// Actions
void UHadesInventoryScreen::RefreshActions()
{
	Super::RefreshActions();

	const bool bScreenClosing = IsClosing();
	SetActionBound(OrganizeHandle, OrganizeAction, IsInventoryShown() && !bScreenClosing, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleOrganizeAction));

	// While an item is held, the tab keys turn it instead of changing tabs. Neither works while the screen closes.
	if (TabList)
	{
		const UInventoryUISubsystem* Interaction = GetInteraction();
		const bool bHolding = Interaction && Interaction->IsHolding();
		const FSimpleDelegate Rotate = FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRotateAction);
		TabList->SetListeningForInput(!bHolding && !bScreenClosing);
		SetActionBound(PreviousTabRotateHandle, TabList->GetPreviousTabAction(), bHolding && !bScreenClosing, Rotate, /*bShowInActionBar*/ false);
		SetActionBound(NextTabRotateHandle, TabList->GetNextTabAction(), bHolding && !bScreenClosing, Rotate, /*bShowInActionBar*/ false);
	}
}

void UHadesInventoryScreen::HandleRotateAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RotateHeld();
	}
}

void UHadesInventoryScreen::HandleOrganizeAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	if (!Interaction || !Inventory || Interaction->IsChoosingItem())
	{
		return;
	}

	Interaction->CancelHold();
	if (!Inventory->OrganizeInventory())
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
