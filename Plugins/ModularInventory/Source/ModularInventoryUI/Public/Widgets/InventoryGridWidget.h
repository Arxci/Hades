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
class UInventoryItemWidget;
class UInventoryUISubsystem;
struct FInventoryChangeSet;


/**
 * Shows a grid container: a focusable cell widget per cell, the items over them, and where the held item would land.
 * Mouse drops land with the dragged item centered on the cursor; gamepad drops put its top-left corner on the focused cell.
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

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
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

	/** Holds the cells, the items and the preview, and sizes itself to them. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Grid", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	/** Covers the cells the held item would take. Place it inside Grid Canvas. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Grid", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlacementPreview;

private:
	UFUNCTION()
	void HandleTargetChanged();

	UFUNCTION()
	void HandleHeldItemChanged();

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);

	UInventoryUISubsystem* GetInteraction() const;
	bool IsDropOperation(const UDragDropOperation* Operation) const;
	FInventoryPlacement GetDropPlacement(const FVector2D& ScreenPosition) const;

	void Refresh();
	void RebuildCells(FIntPoint GridSize);
	void RefreshItems();
	void RefreshPreview();

	FInventoryContainerHandle Container;
	FIntPoint BuiltGridSize = FIntPoint::ZeroValue;
	EInventoryDropAction PreviewAction = EInventoryDropAction::None;

	/** Where the mouse last dragged over this grid, to re-aim when the held item turns without the mouse moving. */
	TOptional<FVector2D> DragScreenPosition;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryCellWidget>> Cells;

	TMap<FInventoryItemHandle, TWeakObjectPtr<UInventoryItemWidget>> ItemWidgets;

	UPROPERTY(Transient)
	FUserWidgetPool ItemPool;

	FInventoryRegistryListener RegistryListener;
};
