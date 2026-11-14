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
	// Plays like the RE games.
	InteractionStyle.bClickOpensItemMenu = true;
	InteractionStyle.bSnapHeldItem = true;
	InteractionStyle.bSwapPicksUpOther = true;
	InteractionStyle.bClickOnAnyRelease = true;

	MoveHint = LOCTEXT("MoveHint", "Select an area to place this item.");
	CombineHint = LOCTEXT("CombineHint", "Select an item to combine with.");
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
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
	}
	if (ItemBoxList)
	{
		ItemBoxList->SetContainer(GetItemBox());
	}
	SetPage(ItemsPage);
	RefreshActionHint();
}

void UHadesInventoryScreen::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnItemChosen.RemoveDynamic(this, &ThisClass::HandleItemChosen);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
	}
	if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer()))
	{
		Controller->RemovePauseRequest(this);
	}

	Super::NativeOnDeactivated();
}

bool UHadesInventoryScreen::NativeOnHandleBackAction()
{
	// Back may stop the choosing, and the hint only follows the held item by itself.
	const bool bHandled = Super::NativeOnHandleBackAction();
	RefreshActionHint();
	return bHandled;
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
		RefreshActionHint();
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

	UHadesItemMenu* Menu = Layout->PushWidgetToLayerStack<UHadesItemMenu>(HadesGameplayTags::UI_Layer_Modal, ItemMenuClass.Get(), [this, Item](UHadesItemMenu& NewMenu)
	{
		NewMenu.SetScreen(this);
		NewMenu.SetItem(Item, FindItemWidget(Item));
	});
	if (Menu && !Menu->OnDeactivated().IsBoundToObject(this))
	{
		Menu->OnDeactivated().AddUObject(this, &ThisClass::HandleMenuClosed);
	}
}

void UHadesInventoryScreen::OpenShortcutMenu(FInventoryItemHandle Item)
{
	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	if (!ShortcutMenuClass || !Layout)
	{
		return;
	}

	UHadesShortcutMenu* Menu = Layout->PushWidgetToLayerStack<UHadesShortcutMenu>(HadesGameplayTags::UI_Layer_Modal, ShortcutMenuClass.Get(), [this, Item](UHadesShortcutMenu& NewMenu)
	{
		NewMenu.SetItem(Item, FindItemWidget(Item));
	});
	if (Menu && !Menu->OnDeactivated().IsBoundToObject(this))
	{
		Menu->OnDeactivated().AddUObject(this, &ThisClass::HandleMenuClosed);
	}
}

void UHadesInventoryScreen::HandleMenuClosed()
{
	ShowActionHint(FText::GetEmpty());
}

void UHadesInventoryScreen::ShowActionHint(const FText& Hint)
{
	MenuHint = Hint;
	RefreshActionHint();
}

void UHadesInventoryScreen::RefreshActionHint()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const FText& Hint = !MenuHint.IsEmpty() ? MenuHint
		: Interaction && Interaction->IsHolding() ? MoveHint
		: Interaction && Interaction->IsChoosingItem() ? CombineHint
		: IdleHint;

	if (ActionHintText)
	{
		ActionHintText->SetText(Hint);
	}
	if (ActionHint)
	{
		// Hidden, not collapsed, so the page above doesn't move.
		ActionHint->SetVisibility(Hint.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}

void UHadesInventoryScreen::HandleItemChosen(FInventoryItemHandle Item, FInventoryItemHandle Chosen)
{
	RefreshActionHint();

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

	// Back on the items, gamepad focus returns to them. The mouse aims for itself.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	UWidget* FocusTarget = IsInventoryShown() ? GetDesiredFocusTarget() : nullptr;
	if (FocusTarget && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		FocusTarget->SetFocus();
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

void UHadesInventoryScreen::HandleHeldItemChanged()
{
	RefreshActionHint();
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
