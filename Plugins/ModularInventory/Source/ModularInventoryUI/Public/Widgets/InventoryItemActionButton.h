#pragma once

#include "CoreMinimal.h"

#include "CommonButtonBase.h"

#include "InventoryItemActionButton.generated.h"

class UImage;
class UInventoryItemAction;
class UTextBlock;


/**
 * One entry of an item menu, made for an item action: it shows the action's name and icon, and its hotkey through Input Action Widget.
 * An action that can't run shows disabled, with its reason, when the action says to. It can still be highlighted, so the player sees why.
 */
UCLASS(Abstract)
class MODULARINVENTORYUI_API UInventoryItemActionButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Shows the action, and whether it can run now and why not. Item menus call it for the buttons they make. */
	void SetAction(UInventoryItemAction* InAction, const FText& Label, bool bInAvailable, const FText& UnavailableReason);

	UFUNCTION(BlueprintPure, Category = "Inventory|Action Button")
	UInventoryItemAction* GetAction() const { return Action; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Action Button")
	bool IsActionAvailable() const { return bAvailable; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** The action shown changed, or whether it can run did. Style the button here, such as greying out one that can't run. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Action Button")
	void OnActionChanged(bool bIsAvailable);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** The action's icon. Collapsed when it has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action Button", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Icon;

	/** Why the action can't run, shown only while it can't. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Action Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReasonText;

	/** Used instead of Style while the action can't run, such as a greyed-out look. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Action Button")
	TSubclassOf<UCommonButtonStyle> UnavailableStyle;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInventoryItemAction> Action;

	TSubclassOf<UCommonButtonStyle> AvailableStyle;
	bool bAvailable = true;
};
