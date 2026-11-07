#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "InventoryTypes.h"

#include "InventoryCellWidget.generated.h"

class UInventoryUISubsystem;


/**
 * A focusable spot in a container: one grid cell, or one slot. Common UI handles its focus, hover and Accept, so the gamepad
 * and mouse behave like every other menu. Hovering or focusing it aims the held item here; Accept picks up the item here or
 * puts the held one down. Style it like any Common UI button.
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

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget Interface

	UInventoryUISubsystem* GetInteraction() const;
	bool IsTargeted() const;

private:
	void Aim();
	void StopAiming();
	void HandleClicked();

	FInventoryContainerHandle Container;
	FInventoryPlacement Placement;
};
