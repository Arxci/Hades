#include "Widgets/InventoryItemWidget.h"

#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"

#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemDetailsWidget.h"

void UInventoryItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (TagTint)
	{
		DefaultTagTint = TagTint->GetColorAndOpacity();
	}
	DefaultRenderOpacity = GetRenderOpacity();
}

void UInventoryItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.AddUniqueDynamic(this, &ThisClass::HandleSelectedItemChanged);
		Interaction->OnItemPutDown.AddUniqueDynamic(this, &ThisClass::HandleItemPutDown);
	}
	Refresh();
	RefreshInteraction();

	if (bHeldItemShownPending)
	{
		bHeldItemShownPending = false;
		OnHeldItemShown();
	}
}

void UInventoryItemWidget::NativeDestruct()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHoveredItemChanged);
		Interaction->OnSelectedItemChanged.RemoveDynamic(this, &ThisClass::HandleSelectedItemChanged);
		Interaction->OnItemPutDown.RemoveDynamic(this, &ThisClass::HandleItemPutDown);
	}
	InventoryWidgets::CancelLoad(IconHandle);
	InventoryWidgets::CancelLoad(IconShadowHandle);
	ShownDefinition.Reset();

	Super::NativeDestruct();
}

// Content
void UInventoryItemWidget::SetItem(FInventoryItemHandle InItem)
{
	Item = InItem;
	bPreview = false;
	Refresh();

	// Pooled widgets get reused for other items, which may or may not be held or hovered.
	RefreshInteraction();
}

void UInventoryItemWidget::SetCellLayout(float InCellSize, float InCellGap, bool bInRotated)
{
	CellSize = InCellSize;
	CellGap = FMath::Max(0.0f, InCellGap);
	bRotated = bInRotated;
	ApplyCellLayout();
}

void UInventoryItemWidget::ShowHeldItem(float InCellSize, float InCellGap)
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	bDragVisual = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetCellLayout(InCellSize, InCellGap, Interaction && Interaction->IsHeldRotated());
	SetItem(Interaction ? Interaction->GetHeldItem() : FInventoryItemHandle());

	// Not as the held item moves from cell to cell. A drag visual is built only as the drag starts, so it waits until then.
	if (Item.IsSet() && Item != ShownHeldItem)
	{
		ShownHeldItem = Item;
		bHeldItemShownPending = true;
	}
	if (bHeldItemShownPending && GetCachedWidget().IsValid())
	{
		bHeldItemShownPending = false;
		OnHeldItemShown();
	}
}

void UInventoryItemWidget::HideHeldItem()
{
	ShownHeldItem = FInventoryItemHandle();
	bHeldItemShownPending = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UInventoryItemWidget::ShowPreview(const FInventoryItemPreview& InPreview, float InCellSize, float InCellGap)
{
	Item = FInventoryItemHandle();
	Preview = InPreview;
	bPreview = true;
	CellSize = InCellSize;
	CellGap = FMath::Max(0.0f, InCellGap);
	bRotated = InPreview.bRotated;
	Refresh();

	if (bHovered != Preview.bLit)
	{
		bHovered = Preview.bLit;
		OnHoveredChanged(bHovered);
		RefreshLit();
		NativeOnInteractionChanged();
	}
}

const UInventoryItemDefinition* UInventoryItemWidget::GetDefinition() const
{
	if (bPreview)
	{
		return Preview.Definition;
	}

	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	return Inventory ? Inventory->GetItemDefinition(Item) : nullptr;
}

int32 UInventoryItemWidget::GetCount() const
{
	if (bPreview)
	{
		return Preview.Count;
	}

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
		InventoryWidgets::ShowIcon(IconShadow, Definition, IconShadowHandle);
	}
	ApplyCellLayout();

	if (NameText)
	{
		NameText->SetText(Definition ? Definition->DisplayName : FText::GetEmpty());
	}
	if (CountText)
	{
		const int32 Count = GetCount();
		const bool bShowCount = Definition && Definition->GetMaxStackSize() > 1 && (Count > 1 || !bHideCountOfOne);
		CountText->SetText(FText::FormatNamed(CountFormat, TEXT("Count"), FText::AsNumber(Count)));
		CountText->SetVisibility(bShowCount ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailsTooltip)
	{
		DetailsTooltip->SetItem(Item);
	}
	RefreshMeter();
	RefreshTagTint();
	RefreshLit();

	NativeOnItemChanged();
	OnItemChanged();
}

void UInventoryItemWidget::RefreshMeter()
{
	if (!Meter && !MeterText)
	{
		return;
	}

	// A preview has no item, so its values come from the preview and its definition, filled the way the registry fills an item's.
	float Value = 0.0f;
	float Max = 0.0f;
	bool bHasValue = false;
	if (const FInventoryItemPreview* Shown = GetPreview())
	{
		bHasValue = Shown->FindTagValue(MeterTag, Value);
		const FInventoryTagValue* Starting = Shown->Definition ? Shown->Definition->FindTagValue(MeterTag) : nullptr;
		if (!MeterMaxTag.IsValid() || !Shown->FindTagValue(MeterMaxTag, Max))
		{
			Max = Starting ? Starting->Value : 0.0f;
		}
	}
	else if (const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this))
	{
		bHasValue = Inventory->HasTagValue(Item, MeterTag);
		Value = Inventory->GetTagValue(Item, MeterTag);
		Max = Inventory->GetTagValueMax(Item, MeterTag, MeterMaxTag);
	}

	const bool bHasMeter = bHasValue && Max > 0.0f;
	const float Fraction = bHasMeter ? FMath::Clamp(Value / Max, 0.0f, 1.0f) : 0.0f;
	const bool bShown = bHasMeter && !(bHideFullMeter && Fraction >= 1.0f);
	const ESlateVisibility MeterVisibility = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (Meter)
	{
		Meter->SetVisibility(MeterVisibility);
		Meter->SetPercent(Fraction);
		if (bTintMeter)
		{
			Meter->SetFillColorAndOpacity(MeterColors.Pick(Fraction));
		}
	}
	if (MeterText)
	{
		MeterText->SetVisibility(MeterVisibility);
		if (bShown)
		{
			MeterText->SetText(FText::FormatNamed(MeterFormat,
				TEXT("Value"), FText::AsNumber(FMath::RoundToInt(Value)),
				TEXT("Max"), FText::AsNumber(FMath::RoundToInt(Max)),
				TEXT("Percent"), FText::AsNumber(FMath::RoundToInt(Fraction * 100.0f))));
		}
		if (bTintMeter)
		{
			MeterText->SetColorAndOpacity(FSlateColor(MeterColors.Pick(Fraction)));
		}
	}
}

void UInventoryItemWidget::RefreshTagTint()
{
	if (!TagTint)
	{
		return;
	}

	FGameplayTagContainer ItemTags;
	if (const FInventoryItemPreview* Shown = GetPreview())
	{
		ItemTags = Shown->Definition ? Shown->Definition->Tags : FGameplayTagContainer();
	}
	else if (const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this))
	{
		ItemTags = Inventory->GetEffectiveItemTags(Item);
	}
	const FLinearColor* TagColor = InventoryWidgets::FindTagColor(TagColors, ItemTags);
	TagTint->SetColorAndOpacity(TagColor ? *TagColor : DefaultTagTint);
}

void UInventoryItemWidget::ApplyCellLayout()
{
	const UInventoryItemDefinition* Definition = GetDefinition();
	if (!Definition || CellSize <= 0.0f)
	{
		return;
	}

	// The icon has the shape of the item's cells, so it's never stretched. The widget covers the cells and the gaps between them, turned.
	const FVector2D NumCells(Definition->Size);
	const FVector2D Unturned = NumCells * CellSize;
	const FVector2D Area = Unturned + (NumCells - FVector2D(1.0)).ComponentMax(FVector2D::ZeroVector) * CellGap;
	const FVector2D TurnedArea = bRotated ? FVector2D(Area.Y, Area.X) : Area;

	// Laid out unturned in the middle of the widget, then turned around its center, so it covers the item's turned cells.
	const FVector2f Overhang = FVector2f(TurnedArea - Unturned) * 0.5f;

	// The shadow keeps its own Render Transform translation; only the angle is set here.
	for (UImage* Image : { Icon.Get(), IconShadow.Get() })
	{
		if (!Image)
		{
			continue;
		}
		Image->SetDesiredSizeOverride(Unturned);
		Image->SetRenderTransformAngle(bRotated ? 90.0f : 0.0f);

		// The Overlay has the turned size and would squeeze the unturned icon into it, so the icon may hang over its edges; with gaps,
		// the padding also centers it on the widget.
		if (UOverlaySlot* ImageSlot = Cast<UOverlaySlot>(Image->Slot))
		{
			ImageSlot->SetPadding(FMargin(Overhang.X, Overhang.Y));
		}
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

void UInventoryItemWidget::HandleItemPutDown(FInventoryItemHandle PutDownItem, EInventoryHoldEvent Event)
{
	// Only where the item now is, not on what stands in for the held item.
	if (!bDragVisual && !bPreview && Item.IsSet() && PutDownItem == Item)
	{
		OnPutDown(Event);
	}
}

void UInventoryItemWidget::RefreshInteraction()
{
	// A preview keeps the look it was given.
	if (bPreview)
	{
		return;
	}

	// A drag visual stands in for the held item, so it is never held or hovered itself.
	const UInventoryUISubsystem* Interaction = bDragVisual || !Item.IsSet() ? nullptr : GetInteraction();

	bool bChanged = false;
	const bool bNowHeld = Interaction && Interaction->GetHeldItem() == Item;
	if (bNowHeld != bHeld)
	{
		bHeld = bNowHeld;
		bChanged = true;
		SetRenderOpacity(bHeld ? DefaultRenderOpacity * HeldOpacity : DefaultRenderOpacity);
		OnHeldChanged(bHeld);
	}

	// While something is held, its preview shows where it goes, and a drag sends no hover events to keep this up to date.
	const bool bNowHovered = Interaction && !Interaction->IsHolding() && Interaction->GetHoveredItem() == Item;
	if (bNowHovered != bHovered)
	{
		bHovered = bNowHovered;
		bChanged = true;
		OnHoveredChanged(bHovered);
	}

	const bool bNowSelected = Interaction && Interaction->GetSelectedItem() == Item;
	if (bNowSelected != bSelected)
	{
		bSelected = bNowSelected;
		bChanged = true;
		OnSelectedChanged(bSelected);
	}

	if (bChanged)
	{
		RefreshLit();
		NativeOnInteractionChanged();
	}
}

void UInventoryItemWidget::RefreshLit()
{
	if (Background)
	{
		Background->SetVisibility(bDragVisual ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}

	// Hidden, not collapsed, so nothing moves. A drag visual stands in for the held item, so it's never lit itself.
	const bool bLit = !bDragVisual && (bHovered || bSelected);
	for (UWidget* LitPart : { LitBackground.Get(), Glow.Get() })
	{
		if (LitPart)
		{
			LitPart->SetVisibility(bLit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
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
		bPressed = true;
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// While an item is held, the style can make a right-click put it back or turn it instead. The screen does that, anywhere on it.
		// The style can also leave item menus to clicks, so a right-click does nothing here.
		UInventoryUISubsystem* Interaction = GetInteraction();
		const bool bHeldRightClick = Interaction && Interaction->IsHolding() && Interaction->GetStyle().HeldRightClick != EInventoryHeldRightClick::AsUsual;
		if (bHeldRightClick || (Interaction && !Interaction->GetStyle().bRightClickOpensItemMenu))
		{
			return FReply::Unhandled();
		}
		RequestItemMenu();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryItemWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// A click needs the press to have been here too, and the mouse leaving the item forgets the press. A press that turned into a drag
	// ends on the drop target instead, and a release whose press began elsewhere does nothing, such as at the end of a drag whose item
	// was put back with a right-click.
	if (!bDragVisual && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bPressed)
		{
			bPressed = false;
			Click(InMouseEvent);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UInventoryItemWidget::Click(const FPointerEvent& MouseEvent)
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (bDragVisual || !Inventory || !Interaction)
	{
		return;
	}

	const bool bTransfer = MouseEvent.IsShiftDown() && Interaction->GetStyle().bShiftClickTransfers && Interaction->GetTransferContainer().IsSet();
	if (bTransfer)
	{
		Interaction->TransferItem(Item);
	}
	else
	{
		Interaction->ActivatePlacement(Inventory->GetItemContainer(Item), GetPlacementAt(MouseEvent.GetScreenSpacePosition()));
	}
}

void UInventoryItemWidget::RequestItemMenu()
{
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->RequestContext(Item);
	}
	OnContextRequested();
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
	bPressed = false;

	// Only the mouse moving off ends the hover, and with Keep Last Hovered Item, only reaching another item does. A menu opening over
	// the item doesn't, and on a gamepad the focused cell decides, not the hidden cursor Common UI moves along with focus.
	const bool bMovedOff = !GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bKeepHover = Interaction && Interaction->GetStyle().bKeepLastHoveredItem;
	if (!bDragVisual && bMovedOff && !bGamepad && !bKeepHover && Interaction && Interaction->GetHoveredItem() == Item)
	{
		Interaction->SetHoveredItem(FInventoryItemHandle());
	}
}

void UInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPressed = false;
	OutOperation = BeginMouseDrag(InMouseEvent);
}

UInventoryDragDropOperation* UInventoryItemWidget::BeginMouseDrag(const FPointerEvent& MouseEvent)
{
	return BeginDrag(MouseEvent.IsShiftDown() ? FMath::Max(1, GetCount() / 2) : 0);
}

UInventoryDragDropOperation* UInventoryItemWidget::BeginDrag(int32 Count)
{
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	if (bDragVisual || !Interaction || !Inventory || !Interaction->CanBeginHold(Item))
	{
		return nullptr;
	}

	// Aimed at the cell under the cursor first, so that's the cell it's held by, and it's drawn where it lies until the cursor moves on.
	const FVector2D CursorPosition = GetCursorPosition();
	const bool bFromGrid = CellSize > 0.0f;
	if (bFromGrid)
	{
		Interaction->SetTarget(Inventory->GetItemContainer(Item), GetPlacementAt(CursorPosition));
	}
	if (!Interaction->BeginHold(Item, Count))
	{
		return nullptr;
	}

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>();
	Operation->Interaction = Interaction;
	Operation->Pivot = EDragPivot::CenterCenter;

	// Grids draw a snapped held item themselves.
	if (!Interaction->GetStyle().bSnapHeldItem)
	{
		UInventoryItemWidget* Visual = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), DragVisualClass ? DragVisualClass.Get() : GetClass());
		if (Visual)
		{
			Visual->ShowHeldItem(bFromGrid ? CellSize : DragCellSize, bFromGrid ? CellGap : 0.0f);
			Operation->DefaultDragVisual = Visual;
		}

		// From a grid, the cursor keeps the spot it grabbed, as the item lands by the cell under it.
		const FGeometry& Geometry = GetCachedGeometry();
		const FVector2D Size = Geometry.GetLocalSize();
		if (bFromGrid && Size.X > 0.0 && Size.Y > 0.0)
		{
			const FVector2D Grabbed = Geometry.AbsoluteToLocal(CursorPosition);
			Operation->Pivot = EDragPivot::TopLeft;
			Operation->Offset = -(Grabbed / Size).ClampAxes(0.0, 1.0);
		}
	}
	return Operation;
}

FInventoryPlacement UInventoryItemWidget::GetPlacementAt(const FVector2D& ScreenPosition) const
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryPlacement ItemPlacement = Inventory ? Inventory->GetItemPlacement(Item) : FInventoryPlacement();
	const UInventoryItemDefinition* Definition = GetDefinition();
	if (!Definition || CellSize <= 0.0f || bDragVisual)
	{
		return ItemPlacement;
	}

	const FVector2D Local = GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	const FVector2D InCells = (Local + FVector2D(CellGap * 0.5)) / (CellSize + CellGap);
	const FIntPoint LastCell = Definition->GetSize(bRotated) - FIntPoint(1, 1);
	const FIntPoint Offset = FIntPoint(FMath::FloorToInt32(InCells.X), FMath::FloorToInt32(InCells.Y)).ComponentMax(FIntPoint::ZeroValue).ComponentMin(LastCell);
	return FInventoryPlacement::AtCell(ItemPlacement.Cell + Offset);
}

FVector2D UInventoryItemWidget::GetCursorPosition() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	return SlateUser ? FVector2D(SlateUser->GetCursorPosition()) : FVector2D::ZeroVector;
}
