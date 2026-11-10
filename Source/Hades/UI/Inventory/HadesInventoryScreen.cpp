#include "HadesInventoryScreen.h"

#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateUser.h"
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

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryGridWidget.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/InventoryListWidget.h"

#define LOCTEXT_NAMESPACE "HadesInventoryScreen"

UHadesInventoryScreen::UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	InputConfig = EHadesWidgetInputMode::Menu;
	bPauseGame = true;

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

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		// Plays like the RE games.
		Interaction->bClickOpensItemMenu = true;
		Interaction->bSwapPicksUpOther = true;
		Interaction->bSnapHeldItem = true;

		Interaction->OnContextRequested.AddUniqueDynamic(this, &ThisClass::OpenItemMenu);
		Interaction->OnItemChosen.AddUniqueDynamic(this, &ThisClass::HandleItemChosen);
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::RefreshItemActions);
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
		Interaction->OnContextRequested.RemoveDynamic(this, &ThisClass::OpenItemMenu);
		Interaction->OnItemChosen.RemoveDynamic(this, &ThisClass::HandleItemChosen);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::RefreshItemActions);
		Interaction->ClearInteraction();
	}
	CombineItem.Reset();

	Super::NativeOnDeactivated();
}

bool UHadesInventoryScreen::NativeOnHandleBackAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsChoosingItem())
	{
		Interaction->CancelChoosingItem();
		Interaction->SetSelectedItem(FInventoryItemHandle());
		CombineItem.Reset();
		RefreshActionHint();
		return true;
	}
	if (Interaction && Interaction->IsHolding())
	{
		Interaction->CancelHold();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

UWidget* UHadesInventoryScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}
	return MainGrid ? MainGrid->GetCellWidget(FIntPoint::ZeroValue) : nullptr;
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
		CombineItem = Item;
		Interaction->BeginChoosingItem();
		Interaction->SetSelectedItem(Item);
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

void UHadesInventoryScreen::OpenItemMenu(FInventoryItemHandle Item)
{
	bool bHasActions = false;
	for (const EHadesItemAction Action : TEnumRange<EHadesItemAction>())
	{
		bHasActions |= CanRunItemAction(Action, Item);
	}

	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!bHasActions || !ItemMenuClass || !Layout || !Interaction || Interaction->IsChoosingItem() || !IsOnItemsPage())
	{
		return;
	}

	Interaction->CancelHold();
	Interaction->SetSelectedItem(Item);
	const FBox2D Anchor = FindItemAnchor(Item);
	UHadesItemMenu* Menu = Layout->PushWidgetToLayerStack<UHadesItemMenu>(HadesGameplayTags::UI_Layer_Modal, ItemMenuClass.Get(), [this, Item, Anchor](UHadesItemMenu& NewMenu)
	{
		NewMenu.SetItem(this, Item, Anchor);
	});
	if (Menu)
	{
		Menu->OnAnchorDragged.BindUObject(this, &ThisClass::BeginItemDrag, Item);
		if (!Menu->OnDeactivated().IsBoundToObject(this))
		{
			Menu->OnDeactivated().AddUObject(this, &ThisClass::HandleMenuClosed);
		}
	}
}

void UHadesInventoryScreen::OpenShortcutMenu(FInventoryItemHandle Item)
{
	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningLocalPlayer());
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!ShortcutMenuClass || !Layout || !Interaction)
	{
		return;
	}

	Interaction->SetSelectedItem(Item);
	const FBox2D Anchor = FindItemAnchor(Item);
	UHadesShortcutMenu* Menu = Layout->PushWidgetToLayerStack<UHadesShortcutMenu>(HadesGameplayTags::UI_Layer_Modal, ShortcutMenuClass.Get(), [Item, Anchor](UHadesShortcutMenu& NewMenu)
	{
		NewMenu.SetItem(Item, Anchor);
	});
	if (Menu)
	{
		Menu->OnAnchorDragged.BindUObject(this, &ThisClass::BeginItemDrag, Item);
		if (!Menu->OnDeactivated().IsBoundToObject(this))
		{
			Menu->OnDeactivated().AddUObject(this, &ThisClass::HandleMenuClosed);
		}
	}
}

UDragDropOperation* UHadesInventoryScreen::BeginItemDrag(FInventoryItemHandle Item)
{
	UInventoryItemWidget* ItemWidget = MainGrid ? MainGrid->GetItemWidget(Item) : nullptr;
	return ItemWidget ? ItemWidget->BeginDrag() : nullptr;
}

void UHadesInventoryScreen::HandleMenuClosed()
{
	ShowActionHint(FText::GetEmpty());

	// Combining keeps the first item selected while the second is chosen.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem())
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
	}
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

FBox2D UHadesInventoryScreen::FindItemAnchor(FInventoryItemHandle Item) const
{
	if (const UInventoryItemWidget* ItemWidget = MainGrid ? MainGrid->GetItemWidget(Item) : nullptr)
	{
		const FGeometry& Geometry = ItemWidget->GetCachedGeometry();
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
	}

	// Outside the grid, such as in the item box: beside the focused widget on a gamepad, or at the cursor.
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	if (!SlateUser)
	{
		return FBox2D(ForceInit);
	}

	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	const TSharedPtr<SWidget> Focused = SlateUser->GetFocusedWidget();
	if (Focused && Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		const FGeometry& Geometry = Focused->GetCachedGeometry();
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
	}

	const FVector2D CursorPosition = SlateUser->GetCursorPosition();
	return FBox2D(CursorPosition, CursorPosition);
}

void UHadesInventoryScreen::HandleItemChosen(FInventoryItemHandle Item)
{
	const FInventoryItemHandle First = CombineItem;
	CombineItem.Reset();
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
	}
	RefreshActionHint();

	UHadesInventoryComponent* Inventory = GetPlayerInventory();
	if (Inventory && First.IsSet())
	{
		Inventory->CombineItems(First, Item);
	}
}

// Pages
bool UHadesInventoryScreen::IsOnItemsPage() const
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
	if (Interaction && !IsOnItemsPage())
	{
		Interaction->ClearInteraction();
	}
	RefreshItemActions();

	// Back on the items, gamepad focus returns to them. The mouse aims for itself.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	UWidget* FocusTarget = IsOnItemsPage() ? GetDesiredFocusTarget() : nullptr;
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
void UHadesInventoryScreen::RefreshItemActions()
{
	const bool bOnItems = IsOnItemsPage();
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const FInventoryItemHandle Held = Interaction ? Interaction->GetHeldItem() : FInventoryItemHandle();
	const UInventoryItemDefinition* HeldDefinition = Subsystem ? Subsystem->GetItemDefinition(Held) : nullptr;
	const bool bCanSplit = HeldDefinition && HeldDefinition->CanSplit() && Subsystem->GetItemCount(Held) > 1;

	// Move picks up what's under the cursor or focus, or puts down what's held.
	const FInventoryItemHandle Hovered = Interaction ? Interaction->GetHoveredItem() : FInventoryItemHandle();
	const UInventoryContainerLayout* HoveredLayout = Subsystem && Hovered.IsSet() ? Subsystem->GetContainerLayout(Subsystem->GetItemContainer(Hovered)) : nullptr;
	const bool bCanMove = Held.IsSet() || (HoveredLayout && !HoveredLayout->bReadOnlyInUI);

	SetActionBound(MoveHandle, MoveAction, bOnItems && bCanMove, &ThisClass::HandleMoveAction);
	SetActionBound(OrganizeHandle, OrganizeAction, bOnItems, &ThisClass::HandleOrganizeAction);
	SetActionBound(RotateHandle, RotateAction, bOnItems && Interaction && Interaction->CanRotateHeld(), &ThisClass::HandleRotateAction);
	SetActionBound(SplitHandle, SplitAction, bOnItems && bCanSplit, &ThisClass::HandleSplitAction);
}

void UHadesInventoryScreen::HandleHoveredItemChanged(FInventoryItemHandle Item)
{
	RefreshItemActions();
}

void UHadesInventoryScreen::SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, void (UHadesInventoryScreen::*Handler)())
{
	if (bBound && !Handle.IsValid() && !Row.IsNull())
	{
		Handle = RegisterUIActionBinding(FBindUIActionArgs(Row, true, FSimpleDelegate::CreateUObject(this, Handler)));
	}
	else if (!bBound && Handle.IsValid())
	{
		Handle.Unregister();
		Handle = FUIActionBindingHandle();
	}
}

void UHadesInventoryScreen::HandleHeldItemChanged()
{
	RefreshItemActions();
	RefreshActionHint();
}

void UHadesInventoryScreen::HandleMoveAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	if (!Interaction || !Subsystem || Interaction->IsChoosingItem())
	{
		return;
	}

	if (Interaction->IsHolding())
	{
		Interaction->PlaceHeld(Interaction->GetTargetContainer(), Interaction->GetTargetPlacement());
		return;
	}

	const FInventoryItemHandle Item = Interaction->GetHoveredItem();
	if (Interaction->BeginHold(Item))
	{
		// Drawn where it is until the mouse or focus moves.
		Interaction->SetTarget(Subsystem->GetItemContainer(Item), Subsystem->GetItemPlacement(Item));
	}
}

void UHadesInventoryScreen::HandleRotateAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RotateHeld();
	}
}

void UHadesInventoryScreen::HandleSplitAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SplitHeld();
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
UInventoryUISubsystem* UHadesInventoryScreen::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

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
