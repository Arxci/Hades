#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Engine/StreamableManager.h"

#include "InventoryTypes.h"
#include "InventoryUITypes.h"

#include "InventoryItemWidget.generated.h"

class UImage;
class UInventoryDragDropOperation;
class UInventoryItemDefinition;
class UInventoryItemDetailsWidget;
class UInventoryUISubsystem;
class UProgressBar;
class UTextBlock;


/**
 * Shows one item and lets the mouse drag it. Grids position and size it over its cells; elsewhere it takes its icon's size,
 * so put it in a Scale Box to fit a slot.
 * A click does what Accept does on a gamepad: picks the item up or opens its item menu, or puts the held item down on it.
 * In a grid, an item picked up with the mouse is held by the cell under the cursor, which then follows the cursor from cell to cell.
 * It lights up while hovered, focused or selected: Lit Background and Glow show.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call again when the item changes; grids and slots do this for you. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void SetItem(FInventoryItemHandle InItem);

	/**
	 * Sizes the icon to the item's cells and turns it with the item. The widget itself also covers the gaps between its cells, and the
	 * icon is centered on it, so the icon keeps its shape.
	 */
	void SetCellLayout(float InCellSize, float InCellGap, bool bInRotated);

	/** Makes this widget stand in for the held item: it shows the held item, count and turn, and is never held or hovered itself. */
	void ShowHeldItem(float InCellSize, float InCellGap);

	/**
	 * Shows a definition rather than an item, with no inventory needed, such as on a grid's designer preview or in a shop: its icon, the
	 * preview's count, turn and tag values, and drawn hovered if it's lit. Set Item shows a real item again. Cell Size 0 isn't laid out on cells.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	void ShowPreview(const FInventoryItemPreview& InPreview, float InCellSize = 0.0f, float InCellGap = 0.0f);

	/** The preview shown, or null while it shows an item. For marks a subclass draws, such as from the preview's tag values. */
	const FInventoryItemPreview* GetPreview() const { return bPreview ? &Preview : nullptr; }

	/**
	 * Picks the item up and returns the drag that carries it, for a drag that starts on something over the item, such as its menu.
	 * Count 0 is the whole stack. In a grid, it's held by the cell under the cursor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Item")
	UInventoryDragDropOperation* BeginDrag(int32 Count = 0);

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	FInventoryItemHandle GetItem() const { return Item; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	const UInventoryItemDefinition* GetDefinition() const;

	/** On a drag visual, how many are being dragged. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	int32 GetCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsHeld() const { return bHeld; }

	/** The mouse is over it, or gamepad focus is on one of its cells. Never while something is held. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsItemHovered() const { return bHovered; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsDragVisual() const { return bDragVisual; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	//~ End UUserWidget Interface

	/** Before On Item Changed, for C++ subclasses. */
	virtual void NativeOnItemChanged() {}

	/** After On Held Changed, On Hovered Changed or On Selected Changed, for C++ subclasses. */
	virtual void NativeOnInteractionChanged() {}

	/** The item or its state changed. Update what the base doesn't, like a mark for an item that's in hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnItemChanged();

	/** Picked up or put down, by mouse or gamepad. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnHeldChanged(bool bIsHeld);

	/** The mouse is over it, or gamepad focus is on one of its cells. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnHoveredChanged(bool bIsHovered);

	/** It became, or stopped being, the Inventory UI Subsystem's selected item, such as while its item menu is open. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnSelectedChanged(bool bIsSelected);

	/** Right click, after the Inventory UI Subsystem's On Context Requested. Open an item menu from either, for game actions like Use or Drop. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnContextRequested();

	/** Put it straight in an Overlay, centered rather than filling it, so it can turn with the item. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	/**
	 * A copy of Icon behind it: it gets the same texture, size and turn. Tint it dark and offset it with its Render Transform for a
	 * hard drop shadow, so light icons read on light tiles. Place it just before Icon, in the same panel and with the same alignment.
	 * For a soft shadow, have the icon generator bake one into the icons instead (Project Settings, Modular Inventory, Icons, Shadow).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconShadow;

	/**
	 * Covers the item's cells, such as a tile, so a big item reads as one. Hidden on a drag visual and on the held item a grid draws, so
	 * what's under them shows through, such as the placement preview.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Background;

	/** Shown over Background while the item is hovered, focused or selected, such as while its menu is open; hidden otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LitBackground;

	/** Shown while the item is lit, as Lit Background. Give it a negative padding so it reaches past the item. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Glow;

	/** The item's name, such as for an item widget laid out as a row of a list: icon, name, then count. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Collapsed for items that don't stack. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** Collapses Count Text for a stack of one too, so only real stacks show a number. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	bool bHideCountOfOne = false;

	/** What Count Text says. {Count} is the item's count, such as x{Count} for x2. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	FText CountFormat = INVTEXT("{Count}");

	/**
	 * How much of the item still shows in its own spot while it's held, as a fraction of its opacity: 0 hides it, so only the held item
	 * shows, where it would land or under the cursor. 1 leaves it as it is.
	 */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item", meta = (ClampMin = "0", ClampMax = "1"))
	float HeldOpacity = 1.0f;

	/** Tinted by the item's tags, such as a rarity border: the first Tag Colors entry whose tag the item has. Without one, it keeps its own color. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UImage> TagTint;

	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	TArray<FInventoryTagColor> TagColors;

	/** Filled to Meter Tag's value out of its maximum, such as durability or rounds. Collapsed for items without the value. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Meter;

	/** Meter Tag's value in Meter Format. Shown and hidden with the meter. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MeterText;

	/** The tag value Meter and Meter Text show, such as Item.Durability. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Meter")
	FGameplayTag MeterTag;

	/** The tag value that fills the meter, such as Item.MaxDurability. Without it, the value the definition starts Meter Tag at fills it. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Meter")
	FGameplayTag MeterMaxTag;

	/** {Value} is Meter Tag's value, {Max} what fills the meter and {Percent} how full it is, out of 100. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Meter")
	FText MeterFormat = INVTEXT("{Value}");

	/** Hides the meter while it's full, so only worn or spent items show one. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Meter")
	bool bHideFullMeter = false;

	/** Colors Meter and Meter Text by how full the meter is. Off, they keep their own colors. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Meter")
	bool bTintMeter = false;

	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Meter", meta = (EditCondition = "bTintMeter"))
	FInventoryMeterColors MeterColors;

	/** Follows the cursor while dragging, unless the Inventory UI Subsystem's Snap Held Item is on. None uses this widget's own class. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	TSubclassOf<UInventoryItemWidget> DragVisualClass;

	/** Sizes the drag visual when this widget isn't in a grid, such as inside an equipment slot. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item", meta = (ClampMin = "1"))
	float DragCellSize = 64.0f;

	/** Shown on mouse hover. Leave empty when a details panel follows the hovered item instead. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	TSubclassOf<UInventoryItemDetailsWidget> TooltipClass;

private:
	UFUNCTION()
	void HandleHeldItemChanged();

	UFUNCTION()
	void HandleHoveredItemChanged(FInventoryItemHandle HoveredItem);

	UFUNCTION()
	void HandleSelectedItemChanged(FInventoryItemHandle SelectedItem);

	UInventoryUISubsystem* GetInteraction() const;
	void Refresh();
	void RefreshMeter();
	void RefreshTagTint();
	void RefreshInteraction();
	void RefreshLit();
	void ApplyCellLayout();

	/** In a grid, the item's cell at that point on the screen, each cell taking half the gap around it. Elsewhere, the item's own placement. */
	FInventoryPlacement GetPlacementAt(const FVector2D& ScreenPosition) const;
	FVector2D GetCursorPosition() const;

	FInventoryItemHandle Item;
	float CellSize = 0.0f;
	float CellGap = 0.0f;
	bool bRotated = false;
	bool bHeld = false;
	bool bHovered = false;
	bool bSelected = false;
	bool bDragVisual = false;
	bool bPressed = false;
	bool bPreview = false;
	FLinearColor DefaultTagTint = FLinearColor::White;
	float DefaultRenderOpacity = 1.0f;

	UPROPERTY(Transient)
	FInventoryItemPreview Preview;

	TWeakObjectPtr<const UInventoryItemDefinition> ShownDefinition;
	TSharedPtr<FStreamableHandle> IconHandle;
	TSharedPtr<FStreamableHandle> IconShadowHandle;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryItemDetailsWidget> DetailsTooltip;
};
