#include "HadesInventoryScreen.h"

#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "Input/CommonUIInputTypes.h"
#include "PrimaryGameLayout.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/Player/HadesPlayerController.h"
#include "Hades/UI/Foundation/HadesTabButton.h"
#include "Hades/UI/Inventory/HadesItemMenu.h"
#include "Hades/UI/Inventory/HadesShortcutMenu.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Widgets/InventoryListWidget.h"

#define LOCTEXT_NAMESPACE "HadesInventoryScreen"

UHadesInventoryScreen::UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Plays like the RE games: Accept opens an item's menu, and a drag or Move picks it up. With the mouse, a right-click opens the menu,
	// as on a desktop, and a left click does nothing, so items are only moved on purpose.
	InteractionStyle.LeftClick = EInventoryItemPress::None;
	InteractionStyle.RightClick = EInventoryItemPress::OpenItemMenu;
	InteractionStyle.Accept = EInventoryItemPress::OpenItemMenu;
	InteractionStyle.bSnapHeldItem = true;
	InteractionStyle.bSwapPicksUpOther = true;
	InteractionStyle.bKeepLastHoveredItem = true;

	// Opens on the item last lit, or the first pocket, lit and in the details, with the mouse too.
	bAimOnOpen = true;
	bStartOnLastItem = true;

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

	if (Pages)
	{
		RegisterUIActionBinding(FBindUIActionArgs(PreviousTabAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandlePreviousTab)));
		RegisterUIActionBinding(FBindUIActionArgs(NextTabAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleNextTab)));
	}
	for (int32 Index = 0; TabBar && Index < TabBar->GetChildrenCount(); ++Index)
	{
		if (UCommonButtonBase* Tab = Cast<UCommonButtonBase>(TabBar->GetChildAt(Index)))
		{
			Tab->OnClicked().AddUObject(this, &ThisClass::SetPage, Index);
		}
	}
}

void UHadesInventoryScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer()))
	{
		Controller->AddPauseRequest(this);
	}
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemChosen.AddUniqueDynamic(this, &ThisClass::HandleItemChosen);
	}
	if (ItemBoxList)
	{
		ItemBoxList->SetContainer(GetItemBox());
	}
	SetPage(ItemsPage);
}

void UHadesInventoryScreen::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemChosen.RemoveDynamic(this, &ThisClass::HandleItemChosen);
	}
	if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer()))
	{
		Controller->RemovePauseRequest(this);
	}

	Super::NativeOnDeactivated();
}

// Item actions
bool UHadesInventoryScreen::CanRunItemAction(EHadesItemAction Action, FInventoryItemHandle Item) const
{
	const UHadesInventoryComponent* Inventory = GetPlayerInventory();
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	if (!Inventory || !Subsystem || !Subsystem->IsValidItem(Item))
	{
		return false;
	}

	const FInventoryContainerHandle ItemBox = GetItemBox();
	switch (Action)
	{
	case EHadesItemAction::Shortcut:
	{
		const APawn* Pawn = GetOwningPlayerPawn();
		const UHadesQuickSlotComponent* QuickSlots = Pawn ? Pawn->FindComponentByClass<UHadesQuickSlotComponent>() : nullptr;
		return ShortcutMenuClass && QuickSlots && QuickSlots->CanAssign(Item);
	}
	case EHadesItemAction::Use:     return Inventory->CanUseItem(Item);
	case EHadesItemAction::Equip:   return Inventory->CanDraw(Item) && !Inventory->IsInHand(Item);
	case EHadesItemAction::Unequip: return Inventory->IsInHand(Item);
	case EHadesItemAction::Combine: return Inventory->CanCombine(Item);
	case EHadesItemAction::Discard: return Inventory->CanDiscardItem(Item);
	case EHadesItemAction::Store:   return ItemBox.IsSet() && Inventory->OwnsItem(Item) && !Inventory->IsKeyItem(Item);
	case EHadesItemAction::Take:    return ItemBox.IsSet() && Subsystem->GetRootContainer(Subsystem->GetItemContainer(Item)) == ItemBox;
	}
	return false;
}

void UHadesInventoryScreen::RunItemAction(EHadesItemAction Action, FInventoryItemHandle Item)
{
	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!Interaction || !CanRunItemAction(Action, Item))
	{
		return;
	}

	EInventoryError Error = EInventoryError::None;
	switch (Action)
	{
	case EHadesItemAction::Shortcut:
		OpenShortcutMenu(Item);
		break;
	case EHadesItemAction::Use:
		Inventory->UseItem(Item);
		break;
	case EHadesItemAction::Equip:
		Inventory->DrawItem(Item);
		break;
	case EHadesItemAction::Unequip:
		Inventory->StowItem(Item);
		break;
	case EHadesItemAction::Combine:
		Interaction->BeginChoosingItem(Item);
		break;
	case EHadesItemAction::Discard:
		Inventory->DiscardItem(Item);
		break;
	case EHadesItemAction::Store:
		if (!Subsystem->MoveItemToContainers(Item, { GetItemBox() }, 0, false, Error))
		{
			AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("CantStore", "That can't go in the item box."));
		}
		break;
	case EHadesItemAction::Take:
		if (!Inventory->TakeItem(Item, 0, true, Error))
		{
			AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("NoRoomToTake", "Not enough space."));
		}
		break;
	}
}

void UHadesInventoryScreen::NativeOnItemMenuRequested(FInventoryItemHandle Item)
{
	Super::NativeOnItemMenuRequested(Item);

	OpenItemMenu(Item);
}

void UHadesInventoryScreen::OpenItemMenu(FInventoryItemHandle Item)
{
	bool bHasActions = false;
	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		bHasActions |= CanRunItemAction(Action, Item);
	}

	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	if (!bHasActions || !ItemMenuClass || !Layout)
	{
		return;
	}

	// The menu shows its actions' descriptions in this screen's action hint, and gives the hint back as it closes.
	Layout->PushWidgetToLayerStack<UHadesItemMenu>(HadesGameplayTags::UI_Layer_Modal, ItemMenuClass.Get(), [this, Item](UHadesItemMenu& NewMenu)
	{
		NewMenu.SetScreen(this);
		NewMenu.SetItem(Item, FindItemWidget(Item));
	});
}

void UHadesInventoryScreen::OpenShortcutMenu(FInventoryItemHandle Item)
{
	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	if (!ShortcutMenuClass || !Layout)
	{
		return;
	}

	Layout->PushWidgetToLayerStack<UHadesShortcutMenu>(HadesGameplayTags::UI_Layer_Modal, ShortcutMenuClass.Get(), [this, Item](UHadesShortcutMenu& NewMenu)
	{
		NewMenu.SetScreen(this);
		NewMenu.SetItem(Item, FindItemWidget(Item));
	});
}

void UHadesInventoryScreen::HandleItemChosen(FInventoryItemHandle Item, FInventoryItemHandle Chosen)
{
	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	if (Inventory && Item.IsSet())
	{
		Inventory->CombineItems(Item, Chosen);
	}
}

// Pages
bool UHadesInventoryScreen::IsInventoryShown() const
{
	return !Pages || Pages->GetActiveWidgetIndex() == ItemsPage;
}

void UHadesInventoryScreen::SetPage(int32 Index)
{
	if (Pages && Pages->GetWidgetAtIndex(Index))
	{
		Pages->SetActiveWidgetIndex(Index);
	}
	for (int32 TabIndex = 0; TabBar && TabIndex < TabBar->GetChildrenCount(); ++TabIndex)
	{
		if (UHadesTabButton* Tab = Cast<UHadesTabButton>(TabBar->GetChildAt(TabIndex)))
		{
			Tab->SetActive(TabIndex == Index);
		}
	}

	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !IsInventoryShown())
	{
		Interaction->ClearInteraction();
	}
	RefreshActions();

	// Back on the items, gamepad focus returns to them. With the mouse, the item focus would start on is lit, until the mouse moves on.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	UWidget* FocusTarget = IsInventoryShown() ? GetDesiredFocusTarget() : nullptr;
	if (FocusTarget && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		FocusTarget->SetFocus();
	}
	else if (bAimOnOpen)
	{
		AimAtStart();
	}
}

void UHadesInventoryScreen::HandlePreviousTab()
{
	// As in Requiem, the tab buttons turn an item being moved.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsHolding())
	{
		Interaction->RotateHeld();
		return;
	}

	const int32 NumPages = Pages ? Pages->GetNumWidgets() : 0;
	if (NumPages > 0)
	{
		SetPage((Pages->GetActiveWidgetIndex() + NumPages - 1) % NumPages);
	}
}

void UHadesInventoryScreen::HandleNextTab()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsHolding())
	{
		Interaction->RotateHeld();
		return;
	}

	const int32 NumPages = Pages ? Pages->GetNumWidgets() : 0;
	if (NumPages > 0)
	{
		SetPage((Pages->GetActiveWidgetIndex() + 1) % NumPages);
	}
}

// Actions
void UHadesInventoryScreen::RefreshActions()
{
	Super::RefreshActions();

	SetActionBound(OrganizeHandle, OrganizeAction, IsInventoryShown(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleOrganizeAction));
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
	if (!Inventory->OrganizePockets())
	{
		AHadesPlayerController::ShowMessageTo(GetOwningPlayerPawn(), LOCTEXT("CantOrganize", "Couldn't organize your pockets."));
	}
}

void UHadesInventoryScreen::HandleCloseAction()
{
	DeactivateWidget();
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
