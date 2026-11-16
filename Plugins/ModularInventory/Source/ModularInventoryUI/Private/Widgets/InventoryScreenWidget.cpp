#include "Widgets/InventoryScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Sound/SoundBase.h"

#include "Actions/InventoryItemAction.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryGridWidget.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/InventoryListWidget.h"
#include "Widgets/InventorySlotWidget.h"

namespace
{
	UInventoryItemWidget* FindItemWidgetIn(const UUserWidget& Parent, FInventoryItemHandle Item)
	{
		UInventoryItemWidget* Found = nullptr;
		if (!Parent.WidgetTree)
		{
			return nullptr;
		}

		Parent.WidgetTree->ForEachWidget([&Found, Item](UWidget* Widget)
		{
			if (Found)
			{
				return;
			}
			if (const UInventoryGridWidget* Grid = Cast<UInventoryGridWidget>(Widget))
			{
				Found = Grid->GetItemWidget(Item);
			}
			else if (const UInventoryListWidget* List = Cast<UInventoryListWidget>(Widget))
			{
				Found = List->GetItemWidget(Item);
			}
			else if (const UInventorySlotWidget* SlotWidget = Cast<UInventorySlotWidget>(Widget))
			{
				Found = SlotWidget->GetItemWidget(Item);
			}
			else if (const UUserWidget* Group = Cast<UUserWidget>(Widget))
			{
				Found = FindItemWidgetIn(*Group, Item);
			}
		});
		return Found;
	}

	/** Every item widget among Parent's widgets and inside the user widgets among them, such as the items in its grids, lists and slots. */
	void ForEachItemWidgetIn(const UUserWidget& Parent, TFunctionRef<void(UInventoryItemWidget&)> Callback)
	{
		if (!Parent.WidgetTree)
		{
			return;
		}

		Parent.WidgetTree->ForEachWidget([&Callback](UWidget* Widget)
		{
			if (UInventoryItemWidget* ItemWidget = Cast<UInventoryItemWidget>(Widget))
			{
				Callback(*ItemWidget);
			}
			else if (const UUserWidget* Group = Cast<UUserWidget>(Widget))
			{
				ForEachItemWidgetIn(*Group, Callback);
			}
		});
	}

	/**
	 * Whether the item widget shows an item under the point, drawn now, where a press would reach it. Cached geometry is only a first
	 * check, as it's from the last time the widget was drawn: one on a page that's no longer shown keeps it. The path from the window
	 * only finds widgets drawn now, with where they are now, and through it, whether anything on the way lets presses through.
	 */
	bool IsPressableAt(const UInventoryItemWidget& ItemWidget, const FVector2D& ScreenPosition)
	{
		const TSharedPtr<SWidget> SlateWidget = ItemWidget.GetCachedWidget();
		if (!SlateWidget || ItemWidget.IsDragVisual() || !ItemWidget.GetItem().IsSet() || !ItemWidget.GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return false;
		}

		FWidgetPath Path;
		if (!FSlateApplication::Get().FindPathToWidget(SlateWidget.ToSharedRef(), Path) || Path.Widgets.Num() == 0)
		{
			return false;
		}
		const int32 Last = Path.Widgets.Num() - 1;
		if (!Path.Widgets[Last].Geometry.IsUnderLocation(ScreenPosition))
		{
			return false;
		}
		for (int32 Index = 0; Index < Last; ++Index)
		{
			if (!Path.Widgets[Index].Widget->GetVisibility().AreChildrenHitTestVisible())
			{
				return false;
			}
		}
		const EVisibility Visibility = SlateWidget->GetVisibility();
		return Visibility.IsHitTestVisible() || Visibility.AreChildrenHitTestVisible();
	}

	/** The grid cell, list entry or slot for a spot in a container, among Parent's widgets. */
	UInventoryCellWidget* FindCellWidgetIn(const UUserWidget& Parent, FInventoryContainerHandle Container, const FInventoryPlacement& Placement)
	{
		UInventoryCellWidget* Found = nullptr;
		if (!Parent.WidgetTree)
		{
			return nullptr;
		}

		Parent.WidgetTree->ForEachWidget([&Found, Container, &Placement](UWidget* Widget)
		{
			if (Found)
			{
				return;
			}
			if (const UInventoryGridWidget* Grid = Cast<UInventoryGridWidget>(Widget))
			{
				Found = Grid->GetContainer() == Container ? Grid->GetCellWidget(Placement.Cell) : nullptr;
			}
			else if (const UInventoryListWidget* List = Cast<UInventoryListWidget>(Widget))
			{
				Found = List->GetContainer() == Container ? List->GetEntryWidget(Placement.Index) : nullptr;
			}
			else if (UInventoryCellWidget* Cell = Cast<UInventoryCellWidget>(Widget))
			{
				Found = Cell->GetContainer() == Container && Cell->GetPlacement() == Placement ? Cell : nullptr;
			}
			else if (const UUserWidget* Group = Cast<UUserWidget>(Widget))
			{
				Found = FindCellWidgetIn(*Group, Container, Placement);
			}
		});
		return Found;
	}
}

UInventoryScreenWidget::UInventoryScreenWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UInventoryScreenWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(bGameKeepsInput ? ECommonInputMode::All : ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UInventoryScreenWidget::SetTransferContainer(FInventoryContainerHandle Container)
{
	TransferContainer = Container;

	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && IsActivated())
	{
		Interaction->SetTransferContainer(Container);
		RefreshActions();
	}
}

UInventoryItemWidget* UInventoryScreenWidget::FindItemWidget(FInventoryItemHandle Item) const
{
	return Item.IsSet() ? FindItemWidgetIn(*this, Item) : nullptr;
}

UInventoryItemWidget* UInventoryScreenWidget::FindItemWidgetAt(const FVector2D& ScreenPosition) const
{
	UInventoryItemWidget* Found = nullptr;
	ForEachItemWidgetIn(*this, [&Found, &ScreenPosition](UInventoryItemWidget& ItemWidget)
	{
		if (!Found && IsPressableAt(ItemWidget, ScreenPosition))
		{
			Found = &ItemWidget;
		}
	});
	return Found;
}

// Mouse: clicks and the wheel that nothing on the screen used come here.
FReply UInventoryScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && Interaction && Interaction->IsHolding())
	{
		switch (Interaction->GetStyle().HeldRightClick)
		{
		case EInventoryHeldRightClick::PutBack:
			Interaction->CancelHold();
			return FReply::Handled();
		case EInventoryHeldRightClick::Rotate:
			Interaction->RotateHeld();
			return FReply::Handled();
		case EInventoryHeldRightClick::AsUsual:
			break;
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryScreenWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsHolding() && Interaction->GetStyle().bWheelRotatesHeld && InMouseEvent.GetWheelDelta() != 0.0f)
	{
		Interaction->RotateHeld();
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UInventoryScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		PreviousStyle = Interaction->GetStyle();
		Interaction->SetStyle(InteractionStyle);
		Interaction->SetTransferContainer(TransferContainer);

		Interaction->OnContextRequested.AddUniqueDynamic(this, &ThisClass::HandleItemMenuRequested);
		Interaction->OnDefaultActionRequested.AddUniqueDynamic(this, &ThisClass::HandleDefaultActionRequested);
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectedItemChanged);
		Interaction->OnHoldEvent.AddUniqueDynamic(this, &ThisClass::HandleHoldEvent);
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	if (bAimOnOpen)
	{
		AimAtStart();
	}
	RefreshActions();
	RefreshActionHint();
}

void UInventoryScreenWidget::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnContextRequested.RemoveDynamic(this, &ThisClass::HandleItemMenuRequested);
		Interaction->OnDefaultActionRequested.RemoveDynamic(this, &ThisClass::HandleDefaultActionRequested);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.RemoveDynamic(this, &ThisClass::HandleSelectedItemChanged);
		Interaction->OnHoldEvent.RemoveDynamic(this, &ThisClass::HandleHoldEvent);

		LastHoveredItem = Interaction->GetHoveredItem();
		Interaction->ClearInteraction();
		Interaction->SetStyle(PreviousStyle);
		Interaction->SetTransferContainer(FInventoryContainerHandle());
	}
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	// Common UI reuses closed screens, so a chest's container, the last cell aimed at and a menu's hint mustn't carry over to the next opening.
	TransferContainer = FInventoryContainerHandle();
	LastAimedContainer = FInventoryContainerHandle();
	MenuHint = FText::GetEmpty();

	Super::NativeOnDeactivated();
}

bool UInventoryScreenWidget::NativeOnHandleBackAction()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->IsChoosingItem())
	{
		Interaction->CancelChoosingItem();
		return true;
	}
	if (Interaction && Interaction->IsHolding())
	{
		Interaction->CancelHold();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

UWidget* UInventoryScreenWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}

	// Back where the player was, such as on the item whose menu just closed, rather than on the first cell.
	UInventoryCellWidget* LastAimed = LastAimedContainer.IsSet() ? FindCellWidgetIn(*this, LastAimedContainer, LastAimedPlacement) : nullptr;
	if (LastAimed && LastAimed->IsVisible() && LastAimed->GetIsEnabled())
	{
		return LastAimed;
	}

	// Then on the item hovered as the screen last closed, wherever it is now.
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (bStartOnLastItem && Inventory && LastHoveredItem.IsSet())
	{
		const FInventoryContainerHandle Container = Inventory->GetItemContainer(LastHoveredItem);
		UInventoryCellWidget* LastItemCell = Container.IsSet() ? FindCellWidgetIn(*this, Container, Inventory->GetItemPlacement(LastHoveredItem)) : nullptr;
		if (LastItemCell && LastItemCell->IsVisible() && LastItemCell->GetIsEnabled())
		{
			return LastItemCell;
		}
	}
	return MainGrid ? MainGrid->GetCellWidget(FIntPoint::ZeroValue) : nullptr;
}

void UInventoryScreenWidget::AimAtStart()
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventoryCellWidget* Start = Interaction && IsInventoryShown() && !Interaction->IsHolding() ? Cast<UInventoryCellWidget>(GetDesiredFocusTarget()) : nullptr;
	if (Start)
	{
		TGuardValue<bool> Aiming(bAimingAtStart, true);
		Interaction->SetTarget(Start->GetContainer(), Start->GetPlacement());
		Interaction->SetHoveredItem(Start->GetItem());
	}
}

UInventoryUISubsystem* UInventoryScreenWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

// Item menus
void UInventoryScreenWidget::HandleItemMenuRequested(FInventoryItemHandle Item)
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && IsInventoryShown())
	{
		NativeOnItemMenuRequested(Item);
		OnItemMenuRequested(Item);
	}
}

// Actions
void UInventoryScreenWidget::RefreshActions()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bUsable = Interaction && IsInventoryShown();

	// With a gamepad, Accept goes to the focused cell, which aims, so Confirm is only shown. With the mouse, keyboard focus isn't on what
	// the mouse is, so Confirm's key acts on the hovered item itself, and only while it can.
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	SetConfirmShown(!bMouse);
	SetActionBound(ConfirmHandle, ConfirmAction, bUsable && bMouse && Interaction->CanActivateAimed(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmAction));

	SetActionBound(MoveHandle, MoveAction, bUsable && Interaction->CanPickUpOrPlace(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMoveAction));

	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const bool bCanOpenItemMenu = bUsable && Inventory && !Interaction->IsHolding() && !Interaction->IsChoosingItem() && Inventory->IsValidItem(Interaction->GetHoveredItem());
	SetActionBound(ItemMenuHandle, ItemMenuAction, bCanOpenItemMenu, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleItemMenuAction));

	SetActionBound(RotateHandle, RotateAction, bUsable && Interaction->CanRotateHeld(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRotateAction));
	SetActionBound(SplitHandle, SplitAction, bUsable && Interaction->CanSplitHeld(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSplitAction));

	const bool bCanTransfer = bUsable && !Interaction->IsHolding() && Interaction->CanTransferItem(Interaction->GetHoveredItem());
	SetActionBound(TransferHandle, TransferAction, bCanTransfer, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTransferAction));
	SetActionBound(TakeAllHandle, TakeAllAction, bUsable && Interaction->CanTakeAll(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTakeAllAction));
}

void UInventoryScreenWidget::SetActionBound(FUIActionBindingHandle& Handle, const FDataTableRowHandle& Row, bool bBound, const FSimpleDelegate& Handler)
{
	if (bBound && !Handle.IsValid() && !Row.IsNull())
	{
		Handle = RegisterUIActionBinding(FBindUIActionArgs(Row, true, Handler));
	}
	else if (!bBound && Handle.IsValid())
	{
		Handle.Unregister();
		Handle = FUIActionBindingHandle();
	}
}

void UInventoryScreenWidget::HandleInteractionChanged()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && Interaction->GetTargetContainer().IsSet())
	{
		LastAimedContainer = Interaction->GetTargetContainer();
		LastAimedPlacement = Interaction->GetTargetPlacement();
	}
	RefreshActions();
	RefreshActionHint();
}

void UInventoryScreenWidget::HandleSelectedItemChanged(FInventoryItemHandle Item)
{
	// Choosing an item for another selects the one it's for, so the hint follows the choosing too.
	RefreshActionHint();
}

// Item actions
FInventoryItemActionContext UInventoryScreenWidget::MakeActionContext(FInventoryItemHandle Item) const
{
	FInventoryItemActionContext Context;
	Context.Item = Item;
	Context.Player = GetOwningPlayer();
	Context.Screen = const_cast<UInventoryScreenWidget*>(this);
	return Context;
}

bool UInventoryScreenWidget::RunAction(UInventoryItemAction* Action, FInventoryItemHandle Item, int32 Count)
{
	FInventoryItemActionContext Context = MakeActionContext(Item);
	Context.Count = Count;
	if (!Action || !Item.IsSet() || !Action->CanRunFor(Context))
	{
		return false;
	}
	Action->RunFor(Context);
	return true;
}

bool UInventoryScreenWidget::RunDefaultAction(FInventoryItemHandle Item)
{
	if (!DefaultActions || !Item.IsSet())
	{
		return false;
	}

	FInventoryItemActionContext Context = MakeActionContext(Item);
	for (UInventoryItemAction* Action : DefaultActions->Actions)
	{
		if (!Action || !Action->CanRunFor(Context))
		{
			continue;
		}

		// No prompt here, so a count is the one it would start at.
		int32 Min = 0;
		int32 Max = 0;
		int32 Count = 0;
		if (Action->Prompt == EInventoryActionPrompt::Quantity)
		{
			Action->GetQuantityRangeFor(Context, Min, Max, Count);
		}
		Context.Count = Count;
		Action->RunFor(Context);
		return true;
	}
	return false;
}

void UInventoryScreenWidget::HandleDefaultActionRequested(FInventoryItemHandle Item)
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && IsInventoryShown())
	{
		RunDefaultAction(Item);
	}
}

// Hint
void UInventoryScreenWidget::ShowActionHint(const FText& Hint)
{
	MenuHint = Hint;
	RefreshActionHint();
}

void UInventoryScreenWidget::RefreshActionHint()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bHolding = Interaction && Interaction->IsHolding();
	const bool bAimedAtInvalid = bHolding && Interaction->GetTargetAction() == EInventoryDropAction::Invalid;
	const FText* InvalidReason = bAimedAtInvalid ? InvalidHints.Find(Interaction->GetTargetError()) : nullptr;
	const FText& Hint = !MenuHint.IsEmpty() ? MenuHint
		: InvalidReason ? *InvalidReason
		: bAimedAtInvalid && !InvalidHint.IsEmpty() ? InvalidHint
		: bHolding ? MoveHint
		: Interaction && Interaction->IsChoosingItem() ? ChooseHint
		: IdleHint;

	if (ActionHintText)
	{
		ActionHintText->SetText(Hint);
	}
	if (ActionHint)
	{
		// Hidden, not collapsed, so what's around it doesn't move.
		ActionHint->SetVisibility(Hint.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
	if (!Hint.EqualTo(ShownHint))
	{
		ShownHint = Hint;
		OnActionHintChanged(ShownHint);
	}
}

void UInventoryScreenWidget::HandleHoveredItemChanged(FInventoryItemHandle Item)
{
	// Not while an item is held, as items aren't drawn hovered then, nor for the item the screen opens on.
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Item.IsSet() && Interaction && !Interaction->IsHolding() && !bAimingAtStart)
	{
		PlayInventorySound(Sounds.HoverItem);
	}
	RefreshActions();
}

// Sounds
void UInventoryScreenWidget::HandleHoldEvent(EInventoryHoldEvent Event)
{
	switch (Event)
	{
	case EInventoryHoldEvent::PickedUp: PlayInventorySound(Sounds.PickUp); break;
	case EInventoryHoldEvent::Placed:   PlayInventorySound(Sounds.PutDown); break;
	case EInventoryHoldEvent::Merged:   PlayInventorySound(Sounds.Merge ? Sounds.Merge : Sounds.PutDown); break;
	case EInventoryHoldEvent::Swapped:  PlayInventorySound(Sounds.Swap ? Sounds.Swap : Sounds.PutDown); break;
	case EInventoryHoldEvent::Refused:  PlayInventorySound(Sounds.CantPlace); break;
	case EInventoryHoldEvent::Rotated:  PlayInventorySound(Sounds.Rotate); break;
	case EInventoryHoldEvent::Split:    PlayInventorySound(Sounds.Split); break;
	case EInventoryHoldEvent::PutBack:  PlayInventorySound(Sounds.PutBack); break;
	}
}

void UInventoryScreenWidget::PlayInventorySound(USoundBase* Sound) const
{
	// A UI sound, so it plays while the game is paused behind the screen.
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void UInventoryScreenWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshActions();
}

void UInventoryScreenWidget::SetConfirmShown(bool bShown)
{
	if (bShown && !ConfirmShownHandle.IsValid() && !ConfirmAction.IsNull())
	{
		// Not consuming it lets Accept reach the focused cell.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = false;
		ConfirmShownHandle = RegisterUIActionBinding(ConfirmArgs);
	}
	else if (!bShown && ConfirmShownHandle.IsValid())
	{
		ConfirmShownHandle.Unregister();
		ConfirmShownHandle = FUIActionBindingHandle();
	}
}

void UInventoryScreenWidget::HandleConfirmAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->ActivateAimed();
	}
}

void UInventoryScreenWidget::HandleMoveAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->PickUpOrPlace();
	}
}

void UInventoryScreenWidget::HandleItemMenuAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RequestContext(Interaction->GetHoveredItem());
	}
}

void UInventoryScreenWidget::HandleRotateAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RotateHeld();
	}
}

void UInventoryScreenWidget::HandleSplitAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SplitHeld();
	}
}

void UInventoryScreenWidget::HandleTransferAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->TransferItem(Interaction->GetHoveredItem());
		RefreshActions();
	}
}

void UInventoryScreenWidget::HandleTakeAllAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->TakeAll();
		RefreshActions();
	}
}
