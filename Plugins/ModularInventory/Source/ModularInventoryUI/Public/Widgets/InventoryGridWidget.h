#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryGridWidget.generated.h"

class UCanvasPanel;
class UInventoryCellWidget;
class UInventoryGridLinesWidget;
class UInventoryItemWidget;
class UInventoryUISubsystem;
struct FInventoryChangeSet;


/**
 * Shows a grid container: a focusable cell widget per cell, the items over them, and where the held item would land.
 * Mouse drops land with the dragged item centered on the cursor; gamepad drops put its top-left corner on the focused cell.
 * With the Inventory UI Subsystem's Snap Held Item on, it also draws the held item on those cells, using Item Class.
 * The hovered or selected item is drawn above the others, so a glow around it can spill over its neighbours.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryGridWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows a container yourself, such as a chest's or a backpack's, instead of finding Container Tag on the player. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Grid")
	void SetContainer(FInventoryContainerHandle InContainer);

	UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
	FInventoryContainerHandle GetContainer() const { return Container; }

	/** For gamepad focus, such as a screen's desired focus target. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
	UInventoryCellWidget* GetCellWidget(FIntPoint Cell) const;

	/** The widget showing the item, such as to open a menu beside it. Null when the item isn't in this grid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
	UInventoryItemWidget* GetItemWidget(FInventoryItemHandle Item) const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	//~ End UUserWidget Interface

	/** The grid was rebuilt for a new or changed container. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Grid")
	void OnContainerRefreshed();

	/** Style Placement Preview by what dropping the held item there would do. None while nothing is aimed at this grid. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Grid")
	void OnPlacementPreviewChanged(EInventoryDropAction Action);

	/** A container of the owning player's inventory component, found on construct, such as Container.Pockets. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	FGameplayTag ContainerTag;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid", meta = (ClampMin = "1"))
	float CellSize = 64.0f;

	/** One per cell, under the items. Needed for gamepads; without it, put Grid Canvas in a Size Box. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	TSubclassOf<UInventoryCellWidget> CellClass;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	TSubclassOf<UInventoryItemWidget> ItemClass;

	/** How fast the preview, and a snapped held item, glide to new cells. Higher is snappier; 0 jumps. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid", meta = (ClampMin = "0"))
	float PreviewGlideSpeed = 0.0f;

	/** Draws thin lines along the cell edges, under the cells, sized to the grid as it grows. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines")
	bool bDrawGridLines = false;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines", meta = (EditCondition = "bDrawGridLines"))
	FLinearColor GridLineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines", meta = (EditCondition = "bDrawGridLines", ClampMin = "0.5"))
	float GridLineThickness = 1.0f;

	/** How far the lines run past the grid's outer edges, in pixels. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines", meta = (EditCondition = "bDrawGridLines", ClampMin = "0"))
	float GridLineOverhang = 0.0f;

	/** Holds the cells, the items and the preview, and sizes itself to them. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Grid", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	/** Covers the cells the held item would take, under the held item when it's drawn. Place it inside Grid Canvas. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Grid", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlacementPreview;

private:
	UFUNCTION()
	void HandleTargetChanged();

	UFUNCTION()
	void HandleHeldItemChanged();

	UFUNCTION()
	void HandleLitItemChanged(FInventoryItemHandle Item);

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);

	UInventoryUISubsystem* GetInteraction() const;
	bool IsDropOperation(const UDragDropOperation* Operation) const;
	FInventoryPlacement GetDropPlacement(const FVector2D& ScreenPosition) const;

	void Refresh();
	void RebuildCells(FIntPoint GridSize);
	void RefreshItems();
	void RefreshItemOrder();
	void RefreshGridLines();
	void RefreshPreview();
	void PlacePreview(const FVector2D& Position);

	FInventoryContainerHandle Container;
	FIntPoint BuiltGridSize = FIntPoint::ZeroValue;
	EInventoryDropAction PreviewAction = EInventoryDropAction::None;

	/** Where the preview is drawn, and where it's gliding to, in grid space. Unset while it's hidden. */
	TOptional<FVector2D> PreviewPosition;
	FVector2D PreviewTargetPosition = FVector2D::ZeroVector;

	/** Where the mouse last dragged over this grid, to re-aim when the held item turns without the mouse moving. */
	TOptional<FVector2D> DragScreenPosition;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryCellWidget>> Cells;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryItemWidget> HeldVisual;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryGridLinesWidget> GridLines;

	TMap<FInventoryItemHandle, TWeakObjectPtr<UInventoryItemWidget>> ItemWidgets;

	UPROPERTY(Transient)
	FUserWidgetPool ItemPool;

	FInventoryRegistryListener RegistryListener;
};
