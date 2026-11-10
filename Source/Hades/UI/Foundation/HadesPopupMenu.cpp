#include "HadesPopupMenu.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "InputCoreTypes.h"

UHadesPopupMenu::UHadesPopupMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	InputConfig = EHadesWidgetInputMode::Menu;
}

void UHadesPopupMenu::PlaceBeside(const FBox2D& InAnchor)
{
	Anchor = InAnchor;
	UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(MenuPanel->Slot);
	if (!PanelSlot)
	{
		return;
	}

	FVector2D PixelPosition;
	FVector2D Min;
	FVector2D Max;
	USlateBlueprintLibrary::AbsoluteToViewport(this, Anchor.Min, PixelPosition, Min);
	USlateBlueprintLibrary::AbsoluteToViewport(this, Anchor.Max, PixelPosition, Max);

	const FVector2D ScreenSize = UWidgetLayoutLibrary::GetViewportSize(this) / UWidgetLayoutLibrary::GetViewportScale(this);
	const bool bOpenLeft = Max.X > ScreenSize.X * 0.5;
	const bool bOpenUp = Max.Y > ScreenSize.Y * 0.5;

	PanelSlot->SetAutoSize(true);
	PanelSlot->SetAlignment(FVector2D(bOpenLeft ? 1.0 : 0.0, bOpenUp ? 1.0 : 0.0));
	PanelSlot->SetPosition(FVector2D(bOpenLeft ? Min.X : Max.X, bOpenUp ? Max.Y : Min.Y));
}

FReply UHadesPopupMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (MenuPanel->GetCachedGeometry().IsUnderLocation(Position))
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

void UHadesPopupMenu::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	OutOperation = OnAnchorDragged.IsBound() ? OnAnchorDragged.Execute() : nullptr;
	if (OutOperation)
	{
		DeactivateWidget();
	}
}
