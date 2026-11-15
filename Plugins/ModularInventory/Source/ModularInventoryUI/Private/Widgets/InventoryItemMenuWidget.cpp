#include "Widgets/InventoryItemMenuWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "InventoryUISubsystem.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemWidget.h"
#include "Widgets/InventoryScreenWidget.h"

namespace
{
	FBox2D GetAbsoluteBox(const FGeometry& Geometry)
	{
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
	}

	/** The inventory screen a widget is drawn in: up through its parents, and from the top of each user widget to the user widget. */
	const UInventoryScreenWidget* FindScreenOf(const UWidget* Widget)
	{
		while (Widget)
		{
			if (const UInventoryScreenWidget* Screen = Cast<UInventoryScreenWidget>(Widget))
			{
				return Screen;
			}
			const UWidget* Parent = Widget->GetParent();
			Widget = Parent ? Parent : Widget->GetTypedOuter<UUserWidget>();
		}
		return nullptr;
	}

	/** Presses and lets go of the left mouse button where the cursor is, as Common UI does for a gamepad's Accept while its cursor shows. */
	void ClickAtCursor(ULocalPlayer* LocalPlayer)
	{
		const TSharedPtr<FSlateUser> SlateUser = LocalPlayer && FSlateApplication::IsInitialized() ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
		if (!SlateUser)
		{
			return;
		}

		FSlateApplication& SlateApp = FSlateApplication::Get();
		const FVector2D Position = SlateUser->GetCursorPosition();
		const FModifierKeysState Modifiers = SlateApp.GetPlatformApplication()->GetModifierKeys();
		const uint32 UserIndex = static_cast<uint32>(SlateUser->GetUserIndex());

		const FPointerEvent Press(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Modifiers);
		SlateApp.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), Press);

		const FPointerEvent Release(UserIndex, FSlateApplication::CursorPointerIndex, Position, Position, SlateApp.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Modifiers);
		SlateApp.ProcessMouseButtonUpEvent(Release);
	}
}

UInventoryItemMenuWidget::UInventoryItemMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UInventoryItemMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

void UInventoryItemMenuWidget::SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget)
{
	Item = InItem;
	AnchorWidget = ItemWidget;
	Anchor = FindAnchor(ItemWidget);

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->CancelHold();
		Interaction->SetSelectedItem(Item);
	}

	NativeOnItemChanged();
	OnItemChanged();

	// Last, so the menu is measured with the buttons this item shows.
	PlaceBeside(Anchor);
}

void UInventoryItemMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	RefreshConfirm();

	if (OpenSound)
	{
		UGameplayStatics::PlaySound2D(this, OpenSound);
	}
}

void UInventoryItemMenuWidget::NativeOnDeactivated()
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	if (UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		Input->OnInputMethodChangedNative.RemoveAll(this);
	}

	// Choosing keeps the item selected while another one is picked for it.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && Interaction->GetSelectedItem() == Item)
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
	}
	if (CloseSound)
	{
		UGameplayStatics::PlaySound2D(this, CloseSound);
	}

	Super::NativeOnDeactivated();
}

UWidget* UInventoryItemMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}

	UWidget* FirstButton = nullptr;
	UWidgetTree::ForWidgetAndChildren(MenuPanel, [&FirstButton](UWidget* Widget)
	{
		if (!FirstButton && Widget->IsA<UCommonButtonBase>() && Widget->IsVisible() && Widget->GetIsEnabled())
		{
			FirstButton = Widget;
		}
	});
	return FirstButton;
}

UInventoryUISubsystem* UInventoryItemMenuWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

// Placing
FBox2D UInventoryItemMenuWidget::FindAnchor(const UInventoryItemWidget* ItemWidget) const
{
	if (ItemWidget)
	{
		return GetAbsoluteBox(ItemWidget->GetCachedGeometry());
	}

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
		return GetAbsoluteBox(Focused->GetTickSpaceGeometry());
	}

	const FVector2D CursorPosition = SlateUser->GetCursorPosition();
	return FBox2D(CursorPosition, CursorPosition);
}

void UInventoryItemMenuWidget::PlaceBeside(const FBox2D& Box)
{
	UCanvasPanelSlot* PanelSlot = MenuPanel ? Cast<UCanvasPanelSlot>(MenuPanel->Slot) : nullptr;
	if (!PanelSlot || Placement == EInventoryMenuPlacement::AsDesigned)
	{
		return;
	}

	FVector2D PixelPosition;
	FVector2D Min;
	FVector2D Max;
	USlateBlueprintLibrary::AbsoluteToViewport(this, Box.Min, PixelPosition, Min);
	USlateBlueprintLibrary::AbsoluteToViewport(this, Box.Max, PixelPosition, Max);

	const FVector2D ScreenSize = UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this);
	PanelSlot->SetAutoSize(true);

	if (Placement == EInventoryMenuPlacement::TowardCenter)
	{
		// Toward the middle of the screen, so it never runs off an edge.
		const bool bOpenLeft = Max.X > ScreenSize.X * 0.5;
		const bool bOpenUp = Max.Y > ScreenSize.Y * 0.5;
		PanelSlot->SetAlignment(FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0));
		PanelSlot->SetPosition(FVector2D(bOpenLeft ? Min.X - MenuGap : Max.X + MenuGap, bOpenUp ? Max.Y : Min.Y));
		return;
	}

	// On the side asked for, unless only the other side has room. Level with the item's top, moved up as far as it takes to stay on screen.
	const FVector2D MenuSize = MeasureMenuPanel();
	const bool bRoomRight = Max.X + MenuGap + MenuSize.X <= ScreenSize.X;
	const bool bRoomLeft = Min.X - MenuGap - MenuSize.X >= 0.0;
	const bool bOpenLeft = Placement == EInventoryMenuPlacement::Left ? bRoomLeft || !bRoomRight : !bRoomRight && bRoomLeft;
	const double Left = bOpenLeft ? Min.X - MenuGap - MenuSize.X : Max.X + MenuGap;
	const FVector2D LastPosition = (ScreenSize - MenuSize).ComponentMax(FVector2D::ZeroVector);

	PanelSlot->SetAlignment(FVector2D::ZeroVector);
	PanelSlot->SetPosition(FVector2D(FMath::Clamp(Left, 0.0, LastPosition.X), FMath::Clamp(Min.Y, 0.0, LastPosition.Y)));
}

FVector2D UInventoryItemMenuWidget::MeasureMenuPanel()
{
	// Pushed menus are placed before they're first drawn, so their size isn't known yet without a layout pass.
	TakeWidget();
	ForceLayoutPrepass();
	return MenuPanel ? MenuPanel->GetDesiredSize() : FVector2D::ZeroVector;
}

// Mouse: a right-click anywhere, or a press outside Menu Panel, closes the menu. A left press on an item is also the item's, as if the
// menu weren't open, so the menu waits, as the item would, to see whether it ends in a click or a drag.
FReply UInventoryItemMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		DeactivateWidget();
		return FReply::Handled();
	}

	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(Position))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	UInventoryItemWidget* ItemWidget = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindItemWidgetUnder(Position) : nullptr;
	if (ItemWidget)
	{
		PressedItemWidget = ItemWidget;
		PressedItem = ItemWidget->GetItem();
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	DeactivateWidget();
	return FReply::Handled();
}

FReply UInventoryItemMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInventoryItemWidget* ItemWidget = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? TakePressedItemWidget() : nullptr;
	if (!ItemWidget)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	// A click on the item the menu is beside does nothing. A press on another item closes the menu, and is a click on that item if it's
	// let go over it, as a click must start and end on the item. The menu covered the item, so the item takes the hover first.
	if (ItemWidget != AnchorWidget.Get())
	{
		const bool bClicked = ItemWidget->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
		UInventoryUISubsystem* Interaction = GetInteraction();
		DeactivateWidget();
		if (bClicked)
		{
			if (Interaction)
			{
				Interaction->SetHoveredItem(ItemWidget->GetItem());
			}
			ItemWidget->Click(InMouseEvent);
		}
	}
	return FReply::Handled();
}

void UInventoryItemMenuWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	// The press became a drag of the item it was on, so it won't be a click. Picking the item up closes the menu, and so does a drag on
	// another item that can't be picked up, as any press on it would.
	UInventoryItemWidget* ItemWidget = TakePressedItemWidget();
	if (!ItemWidget)
	{
		return;
	}

	const bool bOtherItem = ItemWidget != AnchorWidget.Get();
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (bOtherItem && Interaction)
	{
		Interaction->SetHoveredItem(ItemWidget->GetItem());
	}
	OutOperation = ItemWidget->BeginMouseDrag(InMouseEvent);
	if (OutOperation || bOtherItem)
	{
		DeactivateWidget();
	}
}

UInventoryItemWidget* UInventoryItemMenuWidget::FindItemWidgetUnder(const FVector2D& ScreenPosition) const
{
	// A pooled item widget may show another item by now.
	UInventoryItemWidget* AnchorItemWidget = AnchorWidget.Get();
	if (AnchorItemWidget && AnchorItemWidget->GetItem() == Item && Anchor.IsInside(ScreenPosition))
	{
		return AnchorItemWidget;
	}

	const UInventoryScreenWidget* Screen = bClickThroughToItems && AnchorItemWidget ? FindScreenOf(AnchorItemWidget) : nullptr;
	return Screen ? Screen->FindItemWidgetAt(ScreenPosition) : nullptr;
}

FReply UInventoryItemMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	RefreshConfirm();
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// Accept goes to the focused button, before any action binding sees the key, but focus doesn't follow the mouse. So with the mouse on a
// button, Accept clicks that one. The menu sees the key before its buttons do.
FReply UInventoryItemMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bMouse = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard;
	if (bMouse && FindHoveredButton() && FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		if (!InKeyEvent.IsRepeat())
		{
			ClickAtCursor(GetOwningLocalPlayer());
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

// Confirm: shown for what Accept does. If focus has left the buttons, its key comes here and clicks the button under the mouse.
void UInventoryItemMenuWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	RefreshConfirm();
}

void UInventoryItemMenuWidget::RefreshConfirm()
{
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bClicks = Input && Input->GetCurrentInputType() == ECommonInputType::MouseAndKeyboard && FindHoveredButton();
	if (ConfirmHandle.IsValid() && bConfirmClicks != bClicks)
	{
		ConfirmHandle.Unregister();
		ConfirmHandle = FUIActionBindingHandle();
	}
	if (!ConfirmHandle.IsValid() && !ConfirmAction.IsNull())
	{
		// Without the mouse on a button, it's only shown: not consuming it lets Accept reach the focused button.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, bClicks ? FSimpleDelegate::CreateUObject(this, &ThisClass::HandleConfirmAction) : FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = bClicks;
		ConfirmHandle = RegisterUIActionBinding(ConfirmArgs);
		bConfirmClicks = bClicks;
	}
}

void UInventoryItemMenuWidget::HandleConfirmAction()
{
	// A real click, so the button does all it does for one, its sounds included.
	if (FindHoveredButton())
	{
		ClickAtCursor(GetOwningLocalPlayer());
	}
}

UCommonButtonBase* UInventoryItemMenuWidget::FindHoveredButton() const
{
	UCommonButtonBase* Hovered = nullptr;
	if (MenuPanel)
	{
		UWidgetTree::ForWidgetAndChildren(MenuPanel, [&Hovered](UWidget* Widget)
		{
			UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget);
			if (!Hovered && Button && Button->IsHovered() && Button->IsVisible() && Button->GetIsEnabled())
			{
				Hovered = Button;
			}
		});
	}
	return Hovered;
}

UInventoryItemWidget* UInventoryItemMenuWidget::TakePressedItemWidget()
{
	UInventoryItemWidget* ItemWidget = PressedItemWidget.Get();
	const bool bSameItem = ItemWidget && PressedItem.IsSet() && ItemWidget->GetItem() == PressedItem;
	PressedItemWidget.Reset();
	PressedItem = FInventoryItemHandle();
	return bSameItem ? ItemWidget : nullptr;
}
