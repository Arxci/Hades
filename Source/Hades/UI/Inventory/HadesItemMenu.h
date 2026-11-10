#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesPopupMenu.h"
#include "Hades/UI/Inventory/HadesInventoryScreen.h"

#include "InventoryTypes.h"

#include "HadesItemMenu.generated.h"

class UCommonButtonBase;
class UTextBlock;


/**
 * What can be done with one item, opened beside it by an inventory screen. Each button only shows when its action can run,
 * and the screen's action hint says what the highlighted one does.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UHadesPopupMenu
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

	/** InAnchor is where the item is on screen, in absolute coordinates. */
	void SetItem(UHadesInventoryScreen* InScreen, FInventoryItemHandle InItem, const FBox2D& InAnchor);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	/** What each action does, shown for the highlighted button. */
	UPROPERTY(EditDefaultsOnly, Category = "Item Menu")
	TMap<EHadesItemAction, FText> Descriptions;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(BlueprintReadOnly, Category = "Item Menu", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ShortcutButton;

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
	void ShowDescription(EHadesItemAction Action);

	TWeakObjectPtr<UHadesInventoryScreen> Screen;
	FInventoryItemHandle Item;
};
