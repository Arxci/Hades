#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Inventory/HadesInventoryScreen.h"

#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesItemMenu.generated.h"

class UCommonButtonBase;
class UTextBlock;


/**
 * What can be done with one item, opened beside it by the inventory screen. Each button only shows when its action can run,
 * and the screen's action hint says what the highlighted one does.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

	/** The screen whose item actions the buttons run. Set it before Set Item. */
	void SetScreen(UHadesInventoryScreen* InScreen) { Screen = InScreen; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UInventoryItemMenuWidget Interface
	virtual void NativeOnItemChanged() override;
	//~ End UInventoryItemMenuWidget Interface

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
};
