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
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

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

	if (OpenSound)
	{
		UGameplayStatics::PlaySound2D(this, OpenSound);
	}
}

void UInventoryItemMenuWidget::NativeOnDeactivated()
{
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
