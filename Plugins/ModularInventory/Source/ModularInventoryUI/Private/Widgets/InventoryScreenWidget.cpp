#include "Widgets/InventoryScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

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

void UInventoryScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Not consuming it lets Accept reach the focused cell, so this only shows in the action bar.
	if (!ConfirmAction.IsNull())
	{
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = false;
		RegisterUIActionBinding(ConfirmArgs);
	}
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
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnHoldEvent.AddUniqueDynamic(this, &ThisClass::HandleHoldEvent);
	}
	RefreshActions();
}

void UInventoryScreenWidget::NativeOnDeactivated()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnContextRequested.RemoveDynamic(this, &ThisClass::HandleItemMenuRequested);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleInteractionChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnHoldEvent.RemoveDynamic(this, &ThisClass::HandleHoldEvent);

		Interaction->ClearInteraction();
		Interaction->SetStyle(PreviousStyle);
		Interaction->SetTransferContainer(FInventoryContainerHandle());
	}

	// Common UI reuses closed screens, so a chest's container and the last cell aimed at mustn't carry over to the next opening.
	TransferContainer = FInventoryContainerHandle();
	LastAimedContainer = FInventoryContainerHandle();

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
	return MainGrid ? MainGrid->GetCellWidget(FIntPoint::ZeroValue) : nullptr;
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

	SetActionBound(MoveHandle, MoveAction, bUsable && Interaction->CanPickUpOrPlace(), FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMoveAction));
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
}

void UInventoryScreenWidget::HandleHoveredItemChanged(FInventoryItemHandle Item)
{
	// Not while an item is held: items aren't drawn hovered then.
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (Item.IsSet() && Interaction && !Interaction->IsHolding())
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

void UInventoryScreenWidget::HandleMoveAction()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->PickUpOrPlace();
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
