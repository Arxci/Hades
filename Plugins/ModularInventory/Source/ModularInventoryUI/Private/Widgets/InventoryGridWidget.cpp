#include "Widgets/InventoryGridWidget.h"

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
	// Bottom to top. The hovered or selected item goes above the others, so a glow around it isn't covered.
	constexpr int32 LinesZOrder = 0;
	constexpr int32 CellZOrder = 1;
	constexpr int32 ItemZOrder = 2;
	constexpr int32 LitItemZOrder = 3;
	constexpr int32 PreviewZOrder = 4;
	constexpr int32 HeldZOrder = 5;
}

UInventoryGridWidget::UInventoryGridWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ItemPool(*this)
{
}

void UInventoryGridWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegistryListener.Bind(this, FOnInventoryRegistryChanged::FDelegate::CreateUObject(this, &ThisClass::HandleRegistryChanged));
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
		Interaction->OnHeldItemChanged.AddUniqueDynamic(this, &ThisClass::HandleHeldItemChanged);
		Interaction->OnHoveredItemChanged.AddUniqueDynamic(this, &ThisClass::HandleLitItemChanged);
		Interaction->OnSelectedItemChanged.AddUniqueDynamic(this, &ThisClass::HandleLitItemChanged);
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
		Interaction->OnHoveredItemChanged.RemoveDynamic(this, &ThisClass::HandleLitItemChanged);
		Interaction->OnSelectedItemChanged.RemoveDynamic(this, &ThisClass::HandleLitItemChanged);
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

// Building
void UInventoryGridWidget::Refresh()
{
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(this);
	const UInventoryContainerLayout* Layout = Inventory ? Inventory->GetContainerLayout(Container) : nullptr;
	const bool bIsGrid = Layout && Layout->Type == EInventoryContainerType::Grid;

	RebuildCells(bIsGrid ? Layout->GridSize : FIntPoint::ZeroValue);
	RefreshItems();
	RefreshPreview();
	OnContainerRefreshed();
}

void UInventoryGridWidget::RebuildCells(FIntPoint GridSize)
{
	if (GridSize != BuiltGridSize)
	{
		for (UInventoryCellWidget* Cell : Cells)
		{
			if (Cell)
			{
				Cell->RemoveFromParent();
			}
		}
		Cells.Reset();
		BuiltGridSize = GridSize;

		for (int32 Y = 0; CellClass && Y < GridSize.Y; ++Y)
		{
			for (int32 X = 0; X < GridSize.X; ++X)
			{
				UInventoryCellWidget* Cell = CreateWidget<UInventoryCellWidget>(this, CellClass);
				UCanvasPanelSlot* CellSlot = GridCanvas->AddChildToCanvas(Cell);
				CellSlot->SetPosition(FVector2D(X, Y) * CellSize);
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

	// The same cells serve whichever container of this size is shown.
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cells[Index]->SetPlacement(Container, FInventoryPlacement::AtCell(FIntPoint(Index % GridSize.X, Index / GridSize.X)));
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
		GridLines = CreateWidget<UInventoryGridLinesWidget>(this, UInventoryGridLinesWidget::StaticClass());
		if (!GridLines)
		{
			return;
		}
		GridLines->SetVisibility(ESlateVisibility::HitTestInvisible);
		GridCanvas->AddChildToCanvas(GridLines)->SetZOrder(LinesZOrder);
	}

	// Sized to the cells, like them, so it never changes the grid's size; the overhang is drawn past its edges.
	GridLines->SetLineStyle(GridLineColor, GridLineThickness, GridLineOverhang);
	GridLines->SetGrid(BuiltGridSize, CellSize);
	if (UCanvasPanelSlot* LinesSlot = Cast<UCanvasPanelSlot>(GridLines->Slot))
	{
		LinesSlot->SetPosition(FVector2D::ZeroVector);
		LinesSlot->SetSize(FVector2D(BuiltGridSize) * CellSize);
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
			ItemSlot->SetPosition(FVector2D(Placement.Cell) * CellSize);
			ItemSlot->SetSize(FVector2D(Inventory->GetItemSize(Item)) * CellSize);
		}
		ItemWidget->SetItem(Item);
		ItemWidget->SetCellLayout(CellSize, Placement.bRotated);
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

		const bool bLit = Entry.Key.IsSet() && (Entry.Key == Hovered || Entry.Key == Selected);
		const int32 ZOrder = bLit ? LitItemZOrder : ItemZOrder;
		if (ItemSlot->GetZOrder() != ZOrder)
		{
			ItemSlot->SetZOrder(ZOrder);
		}
	}
}

void UInventoryGridWidget::HandleLitItemChanged(FInventoryItemHandle Item)
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
	if (HeldVisual)
	{
		HeldVisual->SetVisibility(bShowHeld ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowHeld)
	{
		HeldVisual->ShowHeldItem(CellSize);
	}

	// Sizes change at once. Positions glide there in Native Tick, except when the preview first shows.
	for (UWidget* Widget : { PlacementPreview.Get(), Cast<UWidget>(HeldVisual.Get()) })
	{
		if (UCanvasPanelSlot* PreviewSlot = Widget && bAimedHere ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			PreviewSlot->SetSize(FVector2D(End - Start) * CellSize);
		}
	}
	PreviewTargetPosition = FVector2D(Start) * CellSize;
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
		PreviewAction = Action;
		OnPlacementPreviewChanged(Action);
	}
}

void UInventoryGridWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (PreviewPosition.IsSet() && !PreviewPosition->Equals(PreviewTargetPosition, 0.1))
	{
		PreviewPosition = FMath::Vector2DInterpTo(PreviewPosition.GetValue(), PreviewTargetPosition, InDeltaTime, PreviewGlideSpeed);
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
	// Turning the held item moves its center off the cursor, so aim again from where the mouse is.
	UInventoryUISubsystem* Interaction = GetInteraction();
	if (DragScreenPosition.IsSet() && Interaction && Interaction->IsHolding())
	{
		Interaction->SetTarget(Container, GetDropPlacement(DragScreenPosition.GetValue()));
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
	const FVector2D Local = GridCanvas->GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const FIntPoint HeldSize = Interaction ? Interaction->GetHeldSize() : FIntPoint(1, 1);

	// The dragged item is centered on the cursor.
	const FIntPoint TopLeft(FMath::RoundToInt32(Local.X / CellSize - HeldSize.X * 0.5), FMath::RoundToInt32(Local.Y / CellSize - HeldSize.Y * 0.5));
	return FInventoryPlacement::AtCell(TopLeft);
}

bool UInventoryGridWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (!IsDropOperation(InOperation))
	{
		return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
	}

	const FVector2D ScreenPosition = InDragDropEvent.GetScreenSpacePosition();
	DragScreenPosition = ScreenPosition;
	GetInteraction()->SetTarget(Container, GetDropPlacement(ScreenPosition));
	return true;
}

void UInventoryGridWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	DragScreenPosition.Reset();
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

	DragScreenPosition.Reset();
	return GetInteraction()->PlaceHeld(Container, GetDropPlacement(InDragDropEvent.GetScreenSpacePosition()));
}
