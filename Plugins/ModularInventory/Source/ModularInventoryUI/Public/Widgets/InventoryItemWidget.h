#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
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
 * A left click and a right-click on it do what the Inventory UI Subsystem's style says, such as pick it up or open its item menu, or put the
 * held item down on it. A click is a press and a release on the item, so the release that ends a drag never counts as one.
 * In a grid, an item picked up with the mouse is held by the cell under the cursor, which then follows the cursor from cell to cell.
 * While hovered, focused or selected it's highlighted, as Highlight When Hovered and Highlight When Selected say: Highlight shows and On
 * Highlight Changed fires.
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

	/**
	 * Makes this widget stand in for the held item: it shows the held item, count and turn, and is never held or hovered itself. Each
	 * time it starts showing an item, it gets On Held Item Shown.
	 */
	void ShowHeldItem(float InCellSize, float InCellGap);

	/** Hides a widget standing in for the held item, such as once the held item is aimed elsewhere, until Show Held Item. */
	void HideHeldItem();

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

	// What the mouse does to the item, for a press that something over the item took, such as an item menu: it then acts for the item.
	/** Does what a left click on the item does: with Shift, moves it to the other side where the style allows; otherwise the style's Left Click, on its cell under the cursor. */
	void Click(const FPointerEvent& MouseEvent);

	/**
	 * Does what a right-click on the item does, as the style's Right Click says. False when that's nothing, such as while the style's
	 * Held Right Click leaves right-clicks to the screen.
	 */
	bool RightClick(const FPointerEvent& MouseEvent);

	/** Does what a double-click on the item does, as the style's Double Click says. False when that's None, so the click counts as a click. */
	bool DoubleClick(const FPointerEvent& MouseEvent);

	/** Picks the item up as a left-button drag on it does: the whole stack, or half of it with Shift. */
	UInventoryDragDropOperation* BeginMouseDrag(const FPointerEvent& MouseEvent);

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

	/** Hovered, focused or selected, as Highlight When Hovered and Highlight When Selected say. Never on the stand-in for a held item. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsHighlighted() const { return bHighlighted; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Item")
	bool IsDragVisual() const { return bDragVisual; }

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	//~ End UUserWidget Interface

	/** Before On Item Changed, for C++ subclasses. */
	virtual void NativeOnItemChanged() {}

	/** After On Held Changed, On Hovered Changed, On Selected Changed or On Highlight Changed, for C++ subclasses. */
	virtual void NativeOnInteractionChanged() {}

	/** Before On Value Display Changed, for C++ subclasses. */
	virtual void NativeOnValueDisplayChanged(int32 Index, const FInventoryValueDisplay& Display, bool bShown, float Value, float Max, float Fraction) {}

	/**
	 * Works out a Value Display whose Source is Custom: its number, and what's full, Max 0 for none. Return false to hide it. Display's Tag
	 * says which number, so one function serves several, such as spare ammo for a weapon from the rest of the inventory, or a value from a
	 * fragment. The item is Get Item, or only Get Definition on a designer preview. False by default.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Item")
	bool ResolveCustomValue(const FInventoryValueDisplay& Display, float& OutValue, float& OutMax) const;

	/**
	 * A Value Display started or stopped showing, or its number or what's full changed, including as the widget first shows an item. For
	 * what its named widgets can't do, such as a radial meter's material, or a pulse when ammo runs low. Index is its place in Value
	 * Displays, and Fraction how full it is, from 0 to 1, or 0 without a maximum.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnValueDisplayChanged(int32 Index, const FInventoryValueDisplay& Display, bool bShown, float Value, float Max, float Fraction);

	/** The item or its state changed. Update what the base doesn't, like a mark for an item that's in hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnItemChanged();

	/**
	 * Picked up or put down, by mouse or gamepad. This is the item in its own spot, which Held Opacity can hide; the tile that moves
	 * with the held item is another widget, which gets On Held Item Shown.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnHeldChanged(bool bIsHeld);

	/**
	 * On a widget standing in for the held item, such as the tile a grid draws on the cells it would land on, or a drag visual: it
	 * started showing an item. That's one just picked up, the other item of a swap, or the held item reaching this grid. Play a pick-up
	 * effect here, such as scaling the icon.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnHeldItemShown();

	/**
	 * The held item just went down here: How is Placed on a free spot, Merged onto this stack, Swapped for the item that was here, or
	 * Put Back where it came from. The widget already shows it where it is. Play a put-down effect here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnPutDown(EInventoryHoldEvent How);

	/** The mouse is over it, or gamepad focus is on one of its cells. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnHoveredChanged(bool bIsHovered);

	/** It became, or stopped being, the Inventory UI Subsystem's selected item, such as while its item menu is open. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnSelectedChanged(bool bIsSelected);

	/**
	 * It became, or stopped being, highlighted: hovered, focused or selected, as Highlight When Hovered and Highlight When Selected say. For
	 * whatever Highlight alone can't show, such as an animation.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Item")
	void OnHighlightChanged(bool bIsHighlighted);

	/** Put it straight in an Overlay, centered rather than filling it, so it can turn with the item. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	/**
	 * Covers the item's cells, such as a tile, so a big item reads as one. Hidden on a drag visual and on the held item a grid draws, so
	 * what's under them shows through, such as the placement preview.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Background;

	/**
	 * Shown while the item is highlighted, the item the player points at or acts on, and hidden otherwise. It's only a switch, and how it
	 * looks is up to you: put whatever your game shows for that inside it, such as a lit tile, an outline, a cursor frame, or a glow given a
	 * negative padding so it reaches past the item, and animate it from On Highlight Changed. To show hover and selection differently, turn
	 * one of Highlight When Hovered and Highlight When Selected off and use On Hovered Changed or On Selected Changed for it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Highlight;

	/** Hovered by the mouse, or focused with a gamepad, highlights the item. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	bool bHighlightWhenHovered = true;

	/** Being the selected item highlights it, such as while its item menu is open or it's chosen to use on another. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item")
	bool bHighlightWhenSelected = true;

	/** The item's name, such as for an item widget laid out as a row of a list: icon, name, then count. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** Collapsed for items that don't stack. While a Value Display names it, the Value Displays decide what it shows instead. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Item", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/**
	 * Numbers shown on the item, such as ammo, charges, durability or a stack's count, each in Text Blocks and a Progress Bar you name in
	 * the designer, so any number can be added without code. Each shows only for items it applies to. For one spot that shows whichever
	 * number an item has, give the displays the same Group: the first in the list that applies shows, each in its own widgets if you like,
	 * such as ammo colored by how full the weapon is, else a stack's count. A named widget none shows in is collapsed. Widget names that
	 * don't exist are reported when the widget compiles.
	 */
	UPROPERTY(EditAnywhere, Category = "Inventory|Item|Values", meta = (TitleProperty = "{Group} {Source} {Tag} in {TextWidget}"))
	TArray<FInventoryValueDisplay> ValueDisplays;

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

	UFUNCTION()
	void HandleItemPutDown(FInventoryItemHandle PutDownItem, EInventoryHoldEvent Event);

	UInventoryUISubsystem* GetInteraction() const;

	void Refresh();
	void FindValueDisplayWidgets();
	void RefreshValueDisplays();

	/** Fires On Value Display Changed for a display, if what it shows changed since it last did for the item shown. */
	void ReportValueDisplay(int32 Index, bool bShown, float Value, float Max);

	/** The display's number and what's full, for the item or the preview shown. False when it doesn't apply to it, or is hidden as it is. */
	bool ResolveValueDisplay(const FInventoryValueDisplay& Display, float& OutValue, float& OutMax) const;

	/** Whether the item or the preview shown matches Filter. A preview has no item, so its definition and its values stand in for one. */
	bool MatchesShownItem(const FInventoryItemQuery& Filter) const;
	void RefreshTagTint();
	void RefreshInteraction();
	void RefreshHighlight();
	void ApplyCellLayout();

	/** In a grid, the item's cell at that point on the screen, each cell taking half the gap around it. Elsewhere, the item's own placement. */
	FInventoryPlacement GetPlacementAt(const FVector2D& ScreenPosition) const;
	FVector2D GetCursorPosition() const;

	// Long press: a finger held on the item, on a touch screen, does the style's Long Press, timed in real time so it works while paused.
	void StartLongPress(const FPointerEvent& TouchEvent);
	void StopLongPress();
	void HandleLongPress(const FVector2D& ScreenPosition);

	FTSTicker::FDelegateHandle LongPressHandle;

	/** The finger now down did a long press, so neither its lift nor a drag it starts counts. */
	bool bLongPressed = false;

	FInventoryItemHandle Item;
	float CellSize = 0.0f;
	float CellGap = 0.0f;
	bool bRotated = false;
	bool bHeld = false;
	bool bHovered = false;
	bool bSelected = false;
	bool bHighlighted = false;
	bool bDragVisual = false;
	bool bPressed = false;
	bool bPreview = false;

	/** The item this stand-in last started showing, so On Held Item Shown fires once for it. It waits for the widget to be built. */
	FInventoryItemHandle ShownHeldItem;
	bool bHeldItemShownPending = false;
	FLinearColor DefaultTagTint = FLinearColor::White;
	float DefaultRenderOpacity = 1.0f;

	UPROPERTY(Transient)
	FInventoryItemPreview Preview;

	TWeakObjectPtr<const UInventoryItemDefinition> ShownDefinition;
	TSharedPtr<FStreamableHandle> IconHandle;

	/** The widgets each of Value Displays names, found once, with their designer colors to go back to, and what it last reported. */
	struct FValueDisplayWidgets
	{
		TWeakObjectPtr<UTextBlock> Text;
		TWeakObjectPtr<UProgressBar> Meter;
		TWeakObjectPtr<UWidget> Frame;
		FSlateColor DefaultTextColor;
		FLinearColor DefaultMeterColor = FLinearColor::White;

		/** What On Value Display Changed last said for the item shown, so it only fires again once that changes. */
		bool bReported = false;
		bool bReportedShown = false;
		float ReportedValue = 0.0f;
		float ReportedMax = 0.0f;
	};
	TArray<FValueDisplayWidgets> ValueDisplayWidgets;

	/** What the reports were about. Pooled widgets show other items, which report afresh. */
	FInventoryItemHandle ReportedItem;
	TWeakObjectPtr<const UInventoryItemDefinition> ReportedDefinition;

	/** On Value Display Changed is implemented in Blueprint, so it's worth calling. */
	bool bReportsValueDisplays = false;

	/** A Value Display names Count Text, so the displays decide what it shows. */
	bool bValueDisplaysShowCount = false;

	// The single meter that came before Value Displays. Loaded only so PostLoad can turn a widget's setup into a Value Displays entry.
	UPROPERTY()
	FGameplayTag MeterTag_DEPRECATED;

	UPROPERTY()
	FGameplayTag MeterMaxTag_DEPRECATED;

	UPROPERTY()
	FText MeterFormat_DEPRECATED = INVTEXT("{Value}");

	UPROPERTY()
	bool bHideFullMeter_DEPRECATED = false;

	UPROPERTY()
	bool bTintMeter_DEPRECATED = false;

	UPROPERTY()
	FInventoryMeterColors MeterColors_DEPRECATED;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryItemDetailsWidget> DetailsTooltip;
};
