#include "Widgets/InventoryGridWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

#include "Data/InventoryContainerLayout.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"
#include "InventoryWidgetHelpers.h"
#include "Widgets/InventoryCellWidget.h"
#include "Widgets/InventoryDragDropOperation.h"
#include "Widgets/InventoryItemWidget.h"

namespace
{
	// Bottom to top.
	constexpr int32 CellZOrder = 0;
	constexpr int32 ItemZOrder = 1;
	constexpr int32 PreviewZOrder = 2;
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
	}

	if (UCanvasPanelSlot* PreviewSlot = PlacementPreview ? Cast<UCanvasPanelSlot>(PlacementPreview->Slot) : nullptr)
	{
		PreviewSlot->SetZOrder(PreviewZOrder);
	}

	if (ContainerTag.IsValid())
	{
		Container = InventoryWidgets::FindPlayerContainer(*this, ContainerTag);
	}
	Refresh();
}

void UInventoryGridWidget::NativeDestruct()
{
	RegistryListener.Unbind();
	if (UInventoryUISubsystem* Interaction = GetInteraction())
	{
		Interaction->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
		Interaction->OnHeldItemChanged.RemoveDynamic(this, &ThisClass::HandleHeldItemChanged);
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

UInventoryCellWidget* UInventoryGridWidget::GetCellWidget(FIntPoint Cell) const
{
	const bool bInside = Cell.X >= 0 && Cell.Y >= 0 && Cell.X < BuiltGridSize.X && Cell.Y < BuiltGridSize.Y;
	const int32 CellIndex = Cell.Y * BuiltGridSize.X + Cell.X;
	return bInside && Cells.IsValidIndex(CellIndex) ? Cells[CellIndex].Get() : nullptr;
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
	}

	// The same cells serve whichever container of this size is shown.
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Cells[Index]->SetPlacement(Container, FInventoryPlacement::AtCell(FIntPoint(Index % GridSize.X, Index / GridSize.X)));
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
}

// Preview
void UInventoryGridWidget::RefreshPreview()
{
	const UInventoryUISubsystem* Interaction = GetInteraction();
	const bool bAimedHere = Interaction && Interaction->IsHolding() && Container.IsSet() && Interaction->GetTargetContainer() == Container;

	if (PlacementPreview)
	{
		PlacementPreview->SetVisibility(bAimedHere ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PlacementPreview->Slot);
		if (bAimedHere && PreviewSlot)
		{
			// Clipped to the grid: a preview hanging off the edge would grow the canvas and shift the layout mid-drag.
			const FIntPoint TargetCell = Interaction->GetTargetPlacement().Cell;
			const FIntPoint Start = TargetCell.ComponentMax(FIntPoint::ZeroValue).ComponentMin(BuiltGridSize);
			const FIntPoint End = (TargetCell + Interaction->GetHeldSize()).ComponentMin(BuiltGridSize).ComponentMax(Start);
			PreviewSlot->SetPosition(FVector2D(Start) * CellSize);
			PreviewSlot->SetSize(FVector2D(End - Start) * CellSize);
		}
	}

	const EInventoryDropAction Action = bAimedHere ? Interaction->GetTargetAction() : EInventoryDropAction::None;
	if (Action != PreviewAction)
	{
		PreviewAction = Action;
		OnPlacementPreviewChanged(Action);
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

// Mouse drops
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
	if (Interaction && Interaction->GetTargetContainer() == Container)
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
