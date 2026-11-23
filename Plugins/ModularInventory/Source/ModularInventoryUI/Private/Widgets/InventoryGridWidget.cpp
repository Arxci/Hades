#include "Widgets/InventoryGridWidget.h"

#include "Animation/WidgetAnimation.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"

#include "Data/InventoryContainerLayout.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "ModularInventory.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryGridLinesWidget.h"
#include "Widgets/InventoryItemWidget.h"

namespace
{
	// Bottom to top. The highlighted item goes above the others, so a glow around it isn't covered.
	constexpr int32 LinesZOrder = 0;
	constexpr int32 CellZOrder = 1;
	constexpr int32 ItemZOrder = 2;
	constexpr int32 HighlightedItemZOrder = 3;
	constexpr int32 PreviewZOrder = 4;
	constexpr int32 HeldZOrder = 5;
}

UInventoryGridWidget::UInventoryGridWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ItemPool(*this)
{
}

template <typename WidgetT>
WidgetT* UInventoryGridWidget::MakeChildWidget(TSubclassOf<WidgetT> Class)
{
	WidgetT* Widget = CreateWidget<WidgetT>(this, Class);
#if WITH_EDITOR
	if (Widget && IsDesignTime())
	{
		Widget->SetDesignerFlags(GetDesignerFlags());
	}
#endif
	return Widget;
}

void UInventoryGridWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

#if WITH_EDITOR
	if (IsDesignTime())
	{
		RefreshDesignerPreview();
	}
#endif
}

#if WITH_EDITOR
void UInventoryGridWidget::RefreshDesignerPreview()
{
	if (!GridCanvas)
	{
		return;
	}

	if (!PreviewLayout)
	{
		PreviewLayout = NewObject<UInventoryContainerLayout>(this, NAME_None, RF_Transient);
	}
	// A grid that grows with upgrades, given the preview's added cells, so the designer shows a grown row with its disabled cells.
	PreviewLayout->Type = EInventoryContainerType::Grid;
	PreviewLayout->GridSize = PreviewGridSize.ComponentMax(FIntPoint::ZeroValue);
	PreviewLayout->GridGrowth = EInventoryGridGrowth::WithUpgrades;
	PreviewLayout->MaxRows = 0;

	// Built afresh each time, as the designer runs this again whenever the widget changes.
	ClearCells();
	const bool bHasPreviewGrid = PreviewGridSize.X > 0 && PreviewGridSize.Y > 0;
	const int32 PreviewCapacity = PreviewLayout->GetCellCount() + FMath::Max(0, PreviewAddedCells);
	RebuildCells(bHasPreviewGrid ? PreviewLayout.Get() : nullptr, bHasPreviewGrid ? PreviewLayout->GetGridRowsAtCapacity(PreviewCapacity) : 0, PreviewCapacity);

	for (UInventoryItemWidget* ItemWidget : PreviewItemWidgets)
	{
		if (ItemWidget)
		{
			ItemWidget->RemoveFromParent();
		}
	}
	PreviewItemWidgets.Reset();

	for (const FInventoryItemPreview& Entry : PreviewItems)
	{
		const UInventoryItemDefinition* Definition = Entry.Definition;
		UInventoryItemWidget* ItemWidget = Definition && ItemClass ? MakeChildWidget<UInventoryItemWidget>(ItemClass) : nullptr;
		if (!ItemWidget)
		{
			continue;
		}

		UCanvasPanelSlot* ItemSlot = GridCanvas->AddChildToCanvas(ItemWidget);
		ItemSlot->SetPosition(GetCellPosition(Entry.Cell));
		ItemSlot->SetSize(GetCellsExtent(Definition->GetSize(Entry.bRotated)));
		ItemSlot->SetZOrder(Entry.bHighlighted ? HighlightedItemZOrder : ItemZOrder);
		ItemWidget->ShowPreview(Entry, CellSize, CellGap);
		PreviewItemWidgets.Add(ItemWidget);
	}
}
#endif

void UInventoryGridWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHighlightChanged);
		Interaction->OnSelectedItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHighlightChanged);
	}

	if (UCanvasPanelSlot* PreviewSlot = PlacementPreview ? Cast<UCanvasPanelSlot>(PlacementPreview->Slot) : nullptr)
	{
		PreviewSlot->SetZOrder(PreviewZOrder);
	}

	if (ContainerTag.IsValid())
	{
		if (APlayerController* Player = GetOwningPlayer())
		{
			Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		}
		Container = InventoryWidgets::FindPlayerContainer(*this, ContainerTag);
	}
	Refresh();
}

void UInventoryGridWidget::NativeDestruct()
{
	RegistryListener.Unbind();
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleHighlightChanged);
		Interaction->OnSelectedItemChanged.RemoveDynamic(this, &ThisClass::HandleHighlightChanged);
	}

	Super::NativeDestruct();
}

void UInventoryGridWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ItemPool.ReleaseAllSlateResources();
}

void UInventoryGridWidget::SetContainer(FInventoryContainerHandle InContainer)
{
	Container = InContainer;
	Refresh();
}

void UInventoryGridWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	SetContainer(InventoryWidgets::FindPawnContainer(NewPawn, ContainerTag));
}

UInventoryCellWidget* UInventoryGridWidget::GetCellWidget(FIntPoint Cell) const
{
	const bool bInside = Cell.X >= 0 && Cell.Y >= 0 && Cell.X < BuiltGridSize.X && Cell.Y < BuiltGridSize.Y;
	const int32 CellIndex = Cell.Y * BuiltGridSize.X + Cell.X;
	return bInside && Cells.IsValidIndex(CellIndex) ? Cells[CellIndex].Get() : nullptr;
}

UInventoryItemWidget* UInventoryGridWidget::GetItemWidget(FInventoryItemHandle Item) const
{
	return ItemWidgets.FindRef(Item).Get();
}

UInventoryUISubsystem* UInventoryGridWidget::GetInteraction() const
{
	return UInventoryUISubsystem::Get(GetOwningLocalPlayer());
}

// Layout
FVector2D UInventoryGridWidget::GetCellPosition(const FIntPoint& Cell) const
{
	return FVector2D(Cell) * (CellSize + CellGap);
}

FVector2D UInventoryGridWidget::GetCellsExtent(const FIntPoint& NumCells) const
{
	const FVector2D Count(NumCells.ComponentMax(FIntPoint::ZeroValue));
	return (Count * (CellSize + CellGap) - FVector2D(CellGap)).ComponentMax(FVector2D::ZeroVector);
}

// Building
void UInventoryGridWidget::Refresh()
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const UInventoryContainerLayout* Layout = Inventory ? Inventory->GetContainerLayout(Container) : nullptr;
	const bool bIsGrid = Layout && Layout->Type == EInventoryContainerType::Grid;

	// The rows the container has now, with a free row below them for a grid that grows to fit, and its capacity, the count of usable
	// cells, deciding which are disabled.
	int32 Rows = 0;
	if (bIsGrid)
	{
		const UInventoryUISubsystem* Interaction = GetInteraction();
		Rows = Interaction ? Interaction->GetGridRowsShown(Container) : Inventory->GetGridRows(Container);
	}
	RebuildCells(bIsGrid ? Layout : nullptr, Rows, bIsGrid ? Inventory->GetCapacity(Container) : 0);
	RefreshItems();
	RefreshPreview();
	OnContainerRefreshed();
}

void UInventoryGridWidget::ClearCells()
{
	for (UInventoryCellWidget* Cell : Cells)
	{
		if (Cell)
		{
			Cell->RemoveFromParent();
		}
	}
	Cells.Reset();
	BuiltGridSize = FIntPoint::ZeroValue;
}

void UInventoryGridWidget::RebuildCells(const UInventoryContainerLayout* Layout, int32 Rows, int32 UnlockedCells)
{
	// Whole rows of the layout's columns.
	const FIntPoint GridSize = Layout ? FIntPoint(FMath::Max(0, Layout->GridSize.X), FMath::Max(0, Rows)) : FIntPoint::ZeroValue;

	if (GridSize != BuiltGridSize)
	{
		ClearCells();
		BuiltGridSize = GridSize;

		for (int32 Y = 0; CellClass && Y < GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < GridSize.X; ++X)
			{
				UInventoryCellWidget* Cell = MakeChildWidget<UInventoryCellWidget>(CellClass);
				UCanvasPanelSlot* CellSlot = GridCanvas->AddChildToCanvas(Cell);
				CellSlot->SetPosition(GetCellPosition(FIntPoint(X, Y)));
				CellSlot->SetSize(FVector2D(CellSize));
				CellSlot->SetZOrder(CellZOrder);
				Cells.Add(Cell);
			}
		}

		// Common UI only builds a button around designer content.
		if (!Cells.IsEmpty() && !Cells[0]->GetRootWidget())
		{
			UE_LOG(LogInventory, Warning, TEXT("[Inventory UI] %s: %s has nothing in its designer, so its cells draw nothing and can't be clicked. Give it any root widget, such as an empty Overlay."),
				*GetPathNameSafe(this), *GetNameSafe(CellClass));
		}
	}

	// The same cells serve whichever container of this size is shown, and its capacity, counted row by row from the top left, decides
	// which are disabled. Without a limit, none are.
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FIntPoint Cell(Index % GridSize.X, Index / GridSize.X);
		Cells[Index]->SetPlacement(Container, FInventoryPlacement::AtCell(Cell));
		Cells[Index]->SetLocked(Layout && UnlockedCells != INDEX_NONE && Index >= UnlockedCells);
	}
	RefreshGridLines();
}

void UInventoryGridWidget::RefreshGridLines()
{
	if (!bDrawGridLines)
	{
		if (GridLines)
		{
			GridLines->RemoveFromParent();
			GridLines = nullptr;
		}
		return;
	}

	if (!GridLines)
	{
		GridLines = MakeChildWidget<UInventoryGridLinesWidget>(UInventoryGridLinesWidget::StaticClass());
		if (!GridLines)
		{
			return;
		}
		GridLines->SetVisibility(ESlateVisibility::HitTestInvisible);
		GridCanvas->AddChildToCanvas(GridLines)->SetZOrder(LinesZOrder);
	}

	// Sized to the cells, like them, so it never changes the grid's size; the overhang and the outer half gaps are drawn past its edges.
	GridLines->SetLineStyle(GridLineColor, GridLineThickness, GridLineOverhang);
	GridLines->SetGrid(BuiltGridSize, CellSize, CellGap);
	if (UCanvasPanelSlot* LinesSlot = Cast<UCanvasPanelSlot>(GridLines->Slot))
	{
		LinesSlot->SetPosition(FVector2D::ZeroVector);
		LinesSlot->SetSize(GetCellsExtent(BuiltGridSize));
	}
}

void UInventoryGridWidget::RefreshItems()
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const FInventoryContainerRecord* Record = Inventory ? Inventory->GetRegistry().FindContainer(Container) : nullptr;
	const bool bIsGrid = Record && Record->Layout->Type == EInventoryContainerType::Grid;

	// Copied: item widgets run Blueprint events, which could change the inventory under this loop.
	const TArray<FInventoryItemHandle> ContainerItems = bIsGrid && ItemClass ? Record->Items : TArray<FInventoryItemHandle>();

	for (const FInventoryItemHandle& Item : ContainerItems)
	{
		TWeakObjectPtr<UInventoryItemWidget>& Entry = ItemWidgets.FindOrAdd(Item);
		UInventoryItemWidget* ItemWidget = Entry.Get();
		if (!ItemWidget)
		{
			ItemWidget = ItemPool.GetOrCreateInstance<UInventoryItemWidget>(ItemClass);
			GridCanvas->AddChildToCanvas(ItemWidget)->SetZOrder(ItemZOrder);
			// User widgets ignore the mouse by default, and items are dragged and right-clicked.
			ItemWidget->SetVisibility(ESlateVisibility::Visible);
			Entry = ItemWidget;
		}

		const FInventoryPlacement Placement = Inventory->GetItemPlacement(Item);
		if (UCanvasPanelSlot* ItemSlot = Cast<UCanvasPanelSlot>(ItemWidget->Slot))
		{
			ItemSlot->SetPosition(GetCellPosition(Placement.Cell));
			ItemSlot->SetSize(GetCellsExtent(Inventory->GetItemSize(Item)));
		}
		ItemWidget->SetItem(Item);
		ItemWidget->SetCellLayout(CellSize, CellGap, Placement.bRotated);
	}

	for (auto It = ItemWidgets.CreateIterator(); It; ++It)
	{
		if (!ContainerItems.Contains(It.Key()))
		{
			if (UInventoryItemWidget* ItemWidget = It.Value().Get())
			{
				ItemWidget->RemoveFromParent();
				ItemPool.Release(ItemWidget);
			}
			It.RemoveCurrent();
		}
	}
	RefreshItemOrder();
}

void UInventoryGridWidget::RefreshItemOrder()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const FInventoryItemHandle Hovered = Interaction ? Interaction->GetHoveredItem() : FInventoryItemHandle();
	const FInventoryItemHandle Selected = Interaction ? Interaction->GetSelectedItem() : FInventoryItemHandle();

	for (const TPair<FInventoryItemHandle, TWeakObjectPtr<UInventoryItemWidget>>& Entry : ItemWidgets)
	{
		UInventoryItemWidget* ItemWidget = Entry.Value.Get();
		UCanvasPanelSlot* ItemSlot = ItemWidget ? Cast<UCanvasPanelSlot>(ItemWidget->Slot) : nullptr;
		if (!ItemSlot)
		{
			continue;
		}

		const bool bHighlighted = Entry.Key.IsSet() && (Entry.Key == Hovered || Entry.Key == Selected);
		const int32 ZOrder = bHighlighted ? HighlightedItemZOrder : ItemZOrder;
		if (ItemSlot->GetZOrder() != ZOrder)
		{
			ItemSlot->SetZOrder(ZOrder);
		}
	}
}

void UInventoryGridWidget::HandleHighlightChanged(FInventoryItemHandle Item)
{
	RefreshItemOrder();
}

// Preview
void UInventoryGridWidget::RefreshPreview()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bAimedHere = Interaction && Interaction->IsHolding() && Container.IsSet() && Interaction->GetTargetContainer() == Container;

	// Clipped to the grid: anything hanging off the edge would grow the canvas and shift the layout mid-drag.
	const FIntPoint LandingCell = bAimedHere ? Interaction->GetLandingPlacement().Cell : FIntPoint::ZeroValue;
	const FIntPoint Start = LandingCell.ComponentMax(FIntPoint::ZeroValue).ComponentMin(BuiltGridSize);
	const FIntPoint End = bAimedHere ? (LandingCell + Interaction->GetHeldSize()).ComponentMin(BuiltGridSize).ComponentMax(Start) : Start;

	const bool bShowHeld = bAimedHere && Interaction->GetStyle().bSnapHeldItem && ItemClass;
	if (bShowHeld && !HeldVisual)
	{
		HeldVisual = CreateWidget<UInventoryItemWidget>(this, ItemClass);
		GridCanvas->AddChildToCanvas(HeldVisual)->SetZOrder(HeldZOrder);
	}
	if (PlacementPreview)
	{
		PlacementPreview->SetVisibility(bAimedHere ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (HeldVisual && bShowHeld)
	{
		HeldVisual->ShowHeldItem(CellSize, CellGap);
	}
	else if (HeldVisual)
	{
		HeldVisual->HideHeldItem();
	}

	// Sizes change at once. Positions glide there in Native Tick, except when the preview first shows.
	for (UWidget* Widget : { PlacementPreview.Get(), Cast<UWidget>(HeldVisual.Get()) })
	{
		if (UCanvasPanelSlot* PreviewSlot = Widget && bAimedHere ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			PreviewSlot->SetSize(GetCellsExtent(End - Start));
		}
	}
	PreviewTargetPosition = GetCellPosition(Start);
	if (!bAimedHere)
	{
		PreviewPosition.Reset();
	}
	else if (!PreviewPosition.IsSet() || PreviewGlideSpeed <= 0.0f)
	{
		PreviewPosition = PreviewTargetPosition;
		PlacePreview(PreviewTargetPosition);
	}

	const EInventoryDropAction Action = bAimedHere ? Interaction->GetTargetAction() : EInventoryDropAction::None;
	if (Action != PreviewAction)
	{
		const bool bWasShown = PreviewAction != EInventoryDropAction::None;
		const bool bShown = Action != EInventoryDropAction::None;
		PreviewAction = Action;
		RefreshPreviewLoop();
		OnPlacementPreviewChanged(Action);
		if (bShown != bWasShown)
		{
			OnPlacementPreviewShown(bShown);
		}
	}
}

void UInventoryGridWidget::RefreshPreviewLoop()
{
	if (!PlacementPreviewLoop)
	{
		return;
	}

	if (!PreviewLoopActions.Contains(PreviewAction))
	{
		StopAnimation(PlacementPreviewLoop);
	}
	else if (bRestartPreviewLoop || !IsAnimationPlaying(PlacementPreviewLoop))
	{
		// No end to the loops, and what it animates goes back as it was when it stops.
		PlayAnimation(PlacementPreviewLoop, 0.0f, 0, PreviewLoopPlayMode, PreviewLoopSpeed, true);
	}
}

void UInventoryGridWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PreviewPosition.IsSet() && PreviewPosition.GetValue() != PreviewTargetPosition)
	{
		// Interp To only ever closes in on the target, so the last step lands on it exactly. Left a fraction of a pixel short, its edges
		// can round to other pixels than the cells' edges do, and the preview is drawn a pixel off the cells under it.
		const FVector2D Glided = FMath::Vector2DInterpTo(PreviewPosition.GetValue(), PreviewTargetPosition, InDeltaTime, PreviewGlideSpeed);
		PreviewPosition = Glided.Equals(PreviewTargetPosition, 0.1) ? PreviewTargetPosition : Glided;
		PlacePreview(PreviewPosition.GetValue());
	}
}

void UInventoryGridWidget::PlacePreview(const FVector2D& Position)
{
	for (UWidget* Widget : { PlacementPreview.Get(), Cast<UWidget>(HeldVisual.Get()) })
	{
		if (UCanvasPanelSlot* PreviewSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			PreviewSlot->SetPosition(Position);
		}
	}
}

void UInventoryGridWidget::HandleTargetChanged()
{
	RefreshPreview();
}

void UInventoryGridWidget::HandleHeldItemChanged()
{
	// A grid that grows to fit draws as many free rows as the held item is tall, so picking one up or turning it can change its rows.
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const UInventoryContainerLayout* Layout = Inventory ? Inventory->GetContainerLayout(Container) : nullptr;
	if (Layout && Layout->Type == EInventoryContainerType::Grid && Layout->GridGrowth == EInventoryGridGrowth::ToFitItems)
	{
		Refresh();
		return;
	}
	RefreshPreview();
}

void UInventoryGridWidget::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	if (Changes.Containers.Contains(Container))
	{
		Refresh();
	}
}

// Mouse
FReply UInventoryGridWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Cells under items never see the mouse, so a held item follows the mouse from here. Gamepad focus aims for itself.
	UInventoryUISubsystem* Interaction = GetInteraction();
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	if (Interaction && Interaction->IsHolding() && Container.IsSet() && !bGamepad)
	{
		Interaction->SetTarget(Container, GetDropPlacement(InMouseEvent.GetScreenSpacePosition()));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

bool UInventoryGridWidget::IsDropOperation(const UDragDropOperation* Operation) const
{
	return Operation && Operation->IsA<UInventoryDragDropOperation>() && Container.IsSet() && GetInteraction();
}

FInventoryPlacement UInventoryGridWidget::GetDropPlacement(const FVector2D& ScreenPosition) const
{
	// Cells are a cell and a gap apart. Shifted by half a gap, each cell starts in the middle of the gap before it.
	const FVector2D Local = GridCanvas->GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	const FVector2D InCells = (Local + FVector2D(CellGap * 0.5)) / (CellSize + CellGap);
	return FInventoryPlacement::AtCell(FIntPoint(FMath::FloorToInt32(InCells.X), FMath::FloorToInt32(InCells.Y)));
}

bool UInventoryGridWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (!IsDropOperation(InOperation))
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}

	GetInteraction()->SetTarget(Container, GetDropPlacement(InDragDropEvent.GetScreenSpacePosition()));
	return true;
}

void UInventoryGridWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	UInventoryUISubsystem* Interaction = GetInteraction();
	if (Interaction && !Interaction->GetStyle().bSnapHeldItem && Interaction->GetTargetContainer() == Container)
	{
		Interaction->ClearTarget();
	}
}

bool UInventoryGridWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (!IsDropOperation(InOperation))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	return GetInteraction()->PlaceHeld(Container, GetDropPlacement(InDragDropEvent.GetScreenSpacePosition()));
}
