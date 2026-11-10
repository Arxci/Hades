#include "Widgets/InventoryItemWidget.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemDetailsWidget.h"

void UInventoryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectedItemChanged);
	}
	Refresh();
	RefreshInteraction();
}

void UInventoryItemWidget::NativeDestruct()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.RemoveDynamic(this, &ThisClass::HandleSelectedItemChanged);
	}
	InventoryWidgets::CancelLoad(IconHandle);
	ShownDefinition.Reset();

	Super::NativeDestruct();
}

// Content
void UInventoryItemWidget::SetItem(FInventoryItemHandle InItem)
{
	Item = InItem;
	Refresh();

	// Pooled widgets get reused for other items, which may or may not be held or hovered.
	RefreshInteraction();
}

void UInventoryItemWidget::SetCellLayout(float InCellSize, bool bInRotated)
{
	CellSize = InCellSize;
	bRotated = bInRotated;
	ApplyCellLayout();
}

void UInventoryItemWidget::ShowHeldItem(float InCellSize)
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	bDragVisual = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetCellLayout(InCellSize, Interaction && Interaction->IsHeldRotated());
	SetItem(Interaction ? Interaction->GetHeldItem() : FInventoryItemHandle());
}

const UInventoryItemDefinition* UInventoryItemWidget::GetDefinition() const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	return Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
}

int32 UInventoryItemWidget::GetCount() const
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (bDragVisual && Interaction)
	{
		return Interaction->GetHeldCount();
	}

	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	return Inventory ? Inventory->GetItemCount(Item) : 0;
}

void UInventoryItemWidget::Refresh()
{
	const UInventoryItemDefinition* Definition = GetDefinition();
	if (Definition != ShownDefinition.Get())
	{
		ShownDefinition = Definition;
		InventoryWidgets::ShowIcon(Icon, Definition, IconHandle);
	}
	ApplyCellLayout();

	if (CountText)
	{
		const bool bStackable = Definition && Definition->GetMaxStackSize() > 1;
		CountText->SetText(FText::AsNumber(GetCount()));
		CountText->SetVisibility(bStackable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailsTooltip)
	{
		DetailsTooltip->SetItem(Item);
	}

	NativeOnItemChanged();
	OnItemChanged();
}

void UInventoryItemWidget::ApplyCellLayout()
{
	const UInventoryItemDefinition* Definition = GetDefinition();
	if (!Icon || !Definition || CellSize <= 0.0f)
	{
		return;
	}

	// Laid out unturned and turned around its center, so it covers the item's turned cells.
	const FVector2D Unturned = FVector2D(Definition->Size) * CellSize;
	Icon->SetDesiredSizeOverride(Unturned);
	Icon->SetRenderTransformAngle(bRotated ? 90.0f : 0.0f);

	// The Overlay has the turned size and would squeeze the unturned icon into it, so the icon may hang over its edges.
	if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(Icon->Slot))
	{
		const FVector2D Turned = bRotated ? FVector2D(Unturned.Y, Unturned.X) : Unturned;
		const FVector2f Overhang = FVector2f(Turned - Unturned) * 0.5f;
		IconSlot->SetPadding(FMargin(Overhang.X, Overhang.Y));
	}
}

// Held and hovered
UInventoryUISubsystem* UInventoryItemWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

void UInventoryItemWidget::HandleHeldItemChanged()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	if (bDragVisual && Interaction && Interaction->IsHolding())
	{
		bRotated = Interaction->IsHeldRotated();
		Refresh();
	}
	RefreshInteraction();
}

void UInventoryItemWidget::HandleHoveredItemChanged(FInventoryItemHandle HoveredItem)
{
	RefreshInteraction();
}

void UInventoryItemWidget::HandleSelectedItemChanged(FInventoryItemHandle SelectedItem)
{
	RefreshInteraction();
}

void UInventoryItemWidget::RefreshInteraction()
{
	// A drag visual stands in for the held item, so it is never held or hovered itself.
	const UInventoryUISubsystem* Interaction = bDragVisual || !Item.IsSet() ? nullptr : GetInteraction();

	const bool bNowHeld = Interaction && Interaction->GetHeldItem() == Item;
	if (bNowHeld != bHeld)
	{
		bHeld = bNowHeld;
		OnHeldChanged(bHeld);
	}

	// While something is held, its preview shows where it goes, and a drag sends no hover events to keep this up to date.
	const bool bNowHovered = Interaction && !Interaction->IsHolding() && Interaction->GetHoveredItem() == Item;
	if (bNowHovered != bHovered)
	{
		bHovered = bNowHovered;
		OnHoveredChanged(bHovered);
	}

	const bool bNowSelected = Interaction && Interaction->GetSelectedItem() == Item;
	if (bNowSelected != bSelected)
	{
		bSelected = bNowSelected;
		OnSelectedChanged(bSelected);
	}
}

// Mouse
FReply UInventoryItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragVisual)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (UInventoryUISubsystem* Interaction = GetInteraction())
		{
			Interaction->RequestContext(Item);
		}
		OnContextRequested();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryItemWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A press that turned into a drag ends on the drop target instead, so this is a click. It counts even when the press was taken
	// by a menu over the item and closed it, so a click on another item while a menu is open opens that item's menu.
	if (!bDragVisual && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
		UInventoryUISubsystem* Interaction = GetInteraction();
		if (Inventory && Interaction)
		{
			Interaction->ActivatePlacement(Inventory->GetItemContainer(Item), Inventory->GetItemPlacement(Item));
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UInventoryItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (bDragVisual)
	{
		return;
	}

	if (TooltipClass && !DetailsTooltip)
	{
		DetailsTooltip = CreateWidget<UInventoryItemDetailsWidget>(this, TooltipClass);
		SetToolTip(DetailsTooltip);
	}
	if (DetailsTooltip)
	{
		DetailsTooltip->SetItem(Item);
	}

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->SetHoveredItem(Item);
	}
}

void UInventoryItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	// Only the mouse moving off ends the hover. A menu opening over the item doesn't, and on a gamepad the focused cell decides,
	// not the hidden cursor Common UI moves along with focus.
	const bool bMovedOff = !GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!bDragVisual && bMovedOff && !bGamepad && Interaction && Interaction->GetHoveredItem() == Item)
	{
		Interaction->SetHoveredItem(FInventoryItemHandle());
	}
}

void UInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	// Shift drags half the stack.
	OutOperation = BeginDrag(InMouseEvent.IsShiftDown() ? FMath::Max(1, GetCount() / 2) : 0);
}

UInventoryDragDropOperation* UInventoryItemWidget::BeginDrag(int32 Count)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (bDragVisual || !Interaction || !Interaction->BeginHold(Item, Count))
	{
		return nullptr;
	}

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>();
	Operation->Interaction = Interaction;
	Operation->Pivot = EDragPivot::CenterCenter;

	// Grids draw a snapped held item themselves.
	if (!Interaction->bSnapHeldItem)
	{
		UInventoryItemWidget* Visual = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), DragVisualClass ? DragVisualClass.Get() : GetClass());
		if (Visual)
		{
			Visual->ShowHeldItem(CellSize > 0.0f ? CellSize : DragCellSize);
			Operation->DefaultDragVisual = Visual;
		}
	}
	return Operation;
}
