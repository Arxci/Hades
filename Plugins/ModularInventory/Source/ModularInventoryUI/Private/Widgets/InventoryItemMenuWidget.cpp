#include "Widgets/InventoryItemMenuWidget.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateUser.h"
#include "Input/CommonUIInputTypes.h"
#include "InputCoreTypes.h"

#include "InventoryUISubsystem.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemWidget.h"

namespace
{
	FBox2D GetAbsoluteBox(const FGeometry& Geometry)
	{
		return FBox2D(Geometry.GetAbsolutePosition(), Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
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

void UInventoryItemMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Not consuming it lets Accept reach the focused button, so this only shows in the action bar.
	if (!ConfirmAction.IsNull())
	{
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = false;
		RegisterUIActionBinding(ConfirmArgs);
	}
}

void UInventoryItemMenuWidget::SetItem(FInventoryItemHandle InItem, UInventoryItemWidget* ItemWidget)
{
	Item = InItem;
	AnchorWidget = ItemWidget;
	Anchor = FindAnchor(ItemWidget);
	PlaceBeside(Anchor);

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->CancelHold();
		Interaction->SetSelectedItem(Item);
	}

	NativeOnItemChanged();
	OnItemChanged();
}

void UInventoryItemMenuWidget::NativeOnDeactivated()
{
	// Choosing keeps the item selected while another one is picked for it.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->IsChoosingItem() && Interaction->GetSelectedItem() == Item)
	{
		Interaction->SetSelectedItem(FInventoryItemHandle());
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
	if (!PanelSlot)
	{
		return;
	}

	FVector2D PixelPosition;
	FVector2D Min;
	FVector2D Max;
	USlateBlueprintLibrary::AbsoluteToViewport(this, Box.Min, PixelPosition, Min);
	USlateBlueprintLibrary::AbsoluteToViewport(this, Box.Max, PixelPosition, Max);

	// Toward the middle of the screen, so it never runs off an edge.
	const FVector2D ScreenSize = UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this);
	const bool bOpenLeft = Max.X > ScreenSize.X * 0.5;
	const bool bOpenUp = Max.Y > ScreenSize.Y * 0.5;

	PanelSlot->SetAutoSize(true);
	PanelSlot->SetAlignment(FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0));
	PanelSlot->SetPosition(FVector2D(bOpenLeft ? Min.X : Max.X, bOpenUp ? Max.Y : Min.Y));
}

// Mouse
FReply UInventoryItemMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (MenuPanel && MenuPanel->GetCachedGeometry().IsUnderLocation(Position))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (Anchor.IsInside(Position) && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	DeactivateWidget();
	return FReply::Handled();
}

void UInventoryItemMenuWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	// A pooled item widget may show another item by now.
	UInventoryItemWidget* ItemWidget = AnchorWidget.Get();
	OutOperation = ItemWidget && ItemWidget->GetItem() == Item ? ItemWidget->BeginDrag() : nullptr;
	if (OutOperation)
	{
		DeactivateWidget();
	}
}
