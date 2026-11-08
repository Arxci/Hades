#include "Widgets/InventoryItemWidget.h"

#include "Components/Image.h"
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
	Icon->SetDesiredSizeOverride(FVector2D(Definition->Size) * CellSize);
	Icon->SetRenderTransformAngle(bRotated ? 90.0f : 0.0f);
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

	const bool bNowHovered = Interaction && Interaction->GetHoveredItem() == Item;
	if (bNowHovered != bHovered)
	{
		bHovered = bNowHovered;
		OnHoveredChanged(bHovered);
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
	// A press that turned into a drag ends on the drop target instead, so this is a click.
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

	UInventoryUISubsystem* Interaction = GetInteraction();
	if (!bDragVisual && Interaction && Interaction->GetHoveredItem() == Item)
	{
		Interaction->SetHoveredItem(FInventoryItemHandle());
	}
}

void UInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	// Shift drags half the stack.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const int32 Count = InMouseEvent.IsShiftDown() ? FMath::Max(1, GetCount() / 2) : 0;
	if (!Interaction || !Interaction->BeginHold(Item, Count))
	{
		return;
	}

	UInventoryItemWidget* Visual = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), DragVisualClass ? DragVisualClass.Get() : GetClass());
	if (!Visual)
	{
		Interaction->CancelHold();
		return;
	}
	Visual->bDragVisual = true;
	Visual->SetVisibility(ESlateVisibility::HitTestInvisible);
	Visual->SetCellLayout(CellSize > 0.0f ? CellSize : DragCellSize, Interaction->IsHeldRotated());
	Visual->SetItem(Item);

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>();
	Operation->Interaction = Interaction;
	Operation->DefaultDragVisual = Visual;
	Operation->Pivot = EDragPivot::CenterCenter;
	OutOperation = Operation;
}
