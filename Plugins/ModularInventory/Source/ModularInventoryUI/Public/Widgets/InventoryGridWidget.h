#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryGridWidget.generated.h"

class APawn;
class UCanvasPanel;
class UInventoryCellWidget;
class UInventoryContainerLayout;
class UInventoryGridLinesWidget;
class UInventoryItemWidget;
class UInventoryUISubsystem;
class UWidgetAnimation;
struct FInventoryChangeSet;


/**
 * Shows a grid container: a focusable cell widget per cell, the items over them, and where the held item would land.
 * The held item goes with the cell it was picked up by on the cell under the cursor, or the focused cell. Each cell takes half the gap
 * around it, so the held item moves on as the cursor crosses the middle of a gap.
 * With the Inventory UI Subsystem's Snap Held Item on, it also draws the held item on those cells, using Item Class.
 * The highlighted item is drawn above the others, so a glow around it can spill over its neighbours.
 * Locked cells, past the container's usable cells, are disabled: they draw with the cell style's Disabled brush and take no focus. They
 * unlock as the container's usable cells grow, by Set Usable Cell Count or a layout with more Usable Cells. With Hide Locked Rows, rows
 * that are wholly locked aren't drawn at all, so the grid grows a row at a time.
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
	virtual void NativePreConstruct() override;
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

	/** Placement Preview appeared, as a held item was aimed at this grid, or went away. After On Placement Preview Changed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Grid")
	void OnPlacementPreviewShown(bool bShown);

	/** A container of the owning player's inventory component, such as Container.Pockets. Found on construct, and again when the player's pawn changes. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	FGameplayTag ContainerTag;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid", meta = (ClampMin = "1"))
	float CellSize = 64.0f;

	/**
	 * Space between neighbouring cells, in pixels. An item bigger than one cell covers the gaps between its cells, so it reads as one tile;
	 * its icon keeps the shape of its cells and is centered on it.
	 */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid", meta = (ClampMin = "0"))
	float CellGap = 0.0f;

	/** One per cell, under the items. Needed for gamepads; without it, put Grid Canvas in a Size Box. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	TSubclassOf<UInventoryCellWidget> CellClass;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	TSubclassOf<UInventoryItemWidget> ItemClass;

	/** How fast the preview, and a snapped held item, glide to new cells. Higher is snappier; 0 jumps. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid", meta = (ClampMin = "0"))
	float PreviewGlideSpeed = 0.0f;

	/**
	 * Rows with no usable cell aren't drawn, so the grid grows a row at a time as its cells unlock, as pockets do in RE2. A row that's only
	 * partly usable still draws its locked cells, disabled. Off, every cell of the grid is drawn, the locked ones disabled, so the player
	 * sees all the room there is to unlock.
	 */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid")
	bool bHideLockedRows = false;

	/** Draws thin lines between the cells, under them, sized to the grid as it grows. With a Cell Gap, they run along the middle of the gaps. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines")
	bool bDrawGridLines = false;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines", meta = (EditCondition = "bDrawGridLines"))
	FLinearColor GridLineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines", meta = (EditCondition = "bDrawGridLines", ClampMin = "0.5"))
	float GridLineThickness = 1.0f;

	/** How far the lines run past the grid's outer edges, in pixels. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Lines", meta = (EditCondition = "bDrawGridLines", ClampMin = "0"))
	float GridLineOverhang = 0.0f;

#if WITH_EDITORONLY_DATA
	/** The grid drawn in the designer, with Cell Class, so you can see your cells and items while you design them. Never in game. 0 draws none. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview", meta = (ClampMin = "0"))
	FIntPoint PreviewGridSize = FIntPoint(4, 3);

	/** Cells past this many, counted row by row, are drawn locked in the designer, as with a layout's Usable Cells. 0 locks none. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview", meta = (ClampMin = "0"))
	int32 PreviewUsableCells = 0;

	/** Items drawn on the designer's grid, with Item Class: each a definition, its cell, its count, and whether it's drawn highlighted. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview", meta = (TitleProperty = "Definition"))
	TArray<FInventoryItemPreview> PreviewItems;
#endif

	/** Holds the cells, the items and the preview, and sizes itself to them. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Grid", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	/** Covers the cells the held item would take, under the held item when it's drawn. Place it inside Grid Canvas. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Grid", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlacementPreview;

	/**
	 * Played on a loop while Placement Preview shows one of Preview Loop Actions, such as a pulse, and stopped otherwise, which puts back
	 * what it animated. Name an animation PlacementPreviewLoop to use one. Don't animate Placement Preview's Brush Color, which
	 * On Placement Preview Changed sets.
	 */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> PlacementPreviewLoop;

	/** What dropping the held item there would do, for Placement Preview Loop to play, such as only Merge, Swap and Invalid. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview Loop")
	TSet<EInventoryDropAction> PreviewLoopActions = { EInventoryDropAction::Place, EInventoryDropAction::Merge, EInventoryDropAction::Swap, EInventoryDropAction::Invalid };

	/** Ping Pong plays it forward and back, such as for a pulse. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview Loop")
	TEnumAsByte<EUMGSequencePlayMode::Type> PreviewLoopPlayMode = EUMGSequencePlayMode::Forward;

	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview Loop", meta = (ClampMin = "0.01"))
	float PreviewLoopSpeed = 1.0f;

	/** Starts the loop over each time the action changes, such as from Swap to Invalid. Off, it carries on. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Grid|Preview Loop")
	bool bRestartPreviewLoop = false;

private:
	UFUNCTION()
	void HandleTargetChanged();

	UFUNCTION()
	void HandleHeldItemChanged();

	UFUNCTION()
	void HandleHighlightChanged(FInventoryItemHandle Item);

	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleRegistryChanged(const FInventoryChangeSet& Changes);

	UInventoryUISubsystem* GetInteraction() const;
	bool IsDropOperation(const UDragDropOperation* Operation) const;

	/** The cell under a point on the screen, each cell taking half the gap around it: where the cursor aims the held item. */
	FInventoryPlacement GetDropPlacement(const FVector2D& ScreenPosition) const;

	/** Where a cell's top-left corner is in Grid Canvas: a cell and a gap from the one before it. */
	FVector2D GetCellPosition(const FIntPoint& Cell) const;

	/** How much room a block of cells takes, with the gaps between them but not around them. */
	FVector2D GetCellsExtent(const FIntPoint& NumCells) const;

	/** Like Create Widget, but in the designer it marks the widget as a designer one too, so it doesn't act as it would in game. */
	template <typename WidgetT>
	WidgetT* MakeChildWidget(TSubclassOf<WidgetT> Class);

#if WITH_EDITOR
	void RefreshDesignerPreview();
#endif

	void Refresh();
	void ClearCells();

	/** Builds a cell for each of the layout's cells, or each row with a usable one with Hide Locked Rows, and locks those past Usable Cells. */
	void RebuildCells(const UInventoryContainerLayout* Layout, int32 UsableCells);
	void RefreshItems();
	void RefreshItemOrder();
	void RefreshGridLines();
	void RefreshPreview();
	void RefreshPreviewLoop();
	void PlacePreview(const FVector2D& Position);

	FInventoryContainerHandle Container;
	FIntPoint BuiltGridSize = FIntPoint::ZeroValue;
	EInventoryDropAction PreviewAction = EInventoryDropAction::None;

	/** Where the preview is drawn, and where it's gliding to, in grid space. Unset while it's hidden. */
	TOptional<FVector2D> PreviewPosition;
	FVector2D PreviewTargetPosition = FVector2D::ZeroVector;

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

#if WITH_EDITORONLY_DATA
	/** Gives the designer's grid its size and locked cells, so its cells are built the way a real container's are. */
	UPROPERTY(Transient)
	TObjectPtr<UInventoryContainerLayout> PreviewLayout;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventoryItemWidget>> PreviewItemWidgets;
#endif
};
