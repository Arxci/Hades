#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "InventoryTypes.h"

#include "InventoryCellWidget.generated.h"

class UInventoryUISubsystem;
enum class ECommonInputType : uint8;


/**
 * A focusable spot in a container: one grid cell, or one slot. Common UI handles its focus, hover and Accept, so the gamepad
 * and mouse behave like every other menu. Hovering or focusing it aims the held item here; Accept picks up the item here or
 * puts the held one down. Style it like any Common UI button.
 * Focus aims while a gamepad is in use, or as the keyboard navigates. With a mouse, focus moved by a screen or menu opening or
 * closing doesn't, so the hover stays where the mouse put it. For the same reason, with the mouse in use, Accept on a focused cell,
 * such as Enter, acts on what the mouse aims at instead of on the cell (Activate Aimed).
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryCellWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|Cell")
	void SetPlacement(FInventoryContainerHandle InContainer, const FInventoryPlacement& InPlacement);

	UFUNCTION(BlueprintPure, Category = "Inventory|Cell")
	FInventoryContainerHandle GetContainer() const { return Container; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Cell")
	FInventoryPlacement GetPlacement() const { return Placement; }

	/** The item covering this cell or sitting in this slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Cell")
	FInventoryItemHandle GetItem() const;

	/**
	 * Locks or unlocks the cell. A locked cell, such as one past its grid layout's Usable Cells, is disabled: it draws with its style's
	 * Disabled brush, can't be clicked and takes no gamepad focus. Grids lock their cells for you.
	 */
	void SetLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "Inventory|Cell")
	bool IsLocked() const { return bLocked; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	UInventoryUISubsystem* GetInteraction() const;
	bool IsTargeted() const;

	/** Used instead of Style while an item is held, such as one without a hover outline, as the held item shows where it goes. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Cell")
	TSubclassOf<UCommonButtonStyle> HoldingStyle;

	/**
	 * Used instead of Style while the player uses the mouse, such as one without a hover look, so empty cells don't react to the mouse.
	 * On a gamepad, Style's hover look is how the player sees which cell is focused, so it's kept there. Holding Style wins while an
	 * item is held. Give it Style's Disabled brush too if the grid has locked cells.
	 */
	UPROPERTY(EditAnywhere, Category = "Inventory|Cell")
	TSubclassOf<UCommonButtonStyle> MouseStyle;

private:
	UFUNCTION()
	void HandleHeldItemChanged();

	void HandleCellInputMethodChanged(ECommonInputType InputType);

	/** Holding Style while an item is held, Mouse Style while the mouse is in use, and Style otherwise, each when it's set. */
	void ApplyCellStyle();

	void HandleHovered();

	/** Aims the held item here and hovers this spot's item. The mouse over an empty spot keeps the hover with Keep Last Hovered Item. */
	void Aim(bool bByFocus);
	void StopAiming();
	void HandleClicked();

	FInventoryContainerHandle Container;
	FInventoryPlacement Placement;
	TSubclassOf<UCommonButtonStyle> IdleStyle;
	bool bLocked = false;
};
