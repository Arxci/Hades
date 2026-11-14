#include "Widgets/InventoryScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Input/CommonUIInputTypes.h"

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

		Interaction->ClearInteraction();
		Interaction->SetStyle(PreviousStyle);
		Interaction->SetTransferContainer(FInventoryContainerHandle());
	}

	// Common UI reuses closed screens, so a chest's container mustn't carry over to the next opening.
	TransferContainer = FInventoryContainerHandle();

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
	RefreshActions();
}

void UInventoryScreenWidget::HandleHoveredItemChanged(FInventoryItemHandle Item)
{
	RefreshActions();
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
