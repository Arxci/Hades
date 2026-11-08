#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"
#include "Hades/UI/Inventory/HadesInventoryScreen.h"

#include "InventoryTypes.h"

#include "HadesItemMenu.generated.h"

class UCommonButtonBase;
class UTextBlock;


/**
 * What can be done with one item, pushed onto the Modal layer by an inventory screen. Each button only shows when its action can run.
 * Give the root a full-screen border that blocks clicks, so the screen behind can't be clicked while the menu is open.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UHadesActivatableWidget
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

	void SetItem(UHadesInventoryScreen* InScreen, FInventoryItemHandle InItem);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> UseButton;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> EquipButton;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> UnequipButton;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CombineButton;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> DiscardButton;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> StoreButton;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> TakeButton;

private:
	UCommonButtonBase* GetButton(EHadesItemAction Action) const;
	void HandleButtonClicked(EHadesItemAction Action);

	TWeakObjectPtr<UHadesInventoryScreen> Screen;
	FInventoryItemHandle Item;
};
