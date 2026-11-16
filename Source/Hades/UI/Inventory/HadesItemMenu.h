#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Inventory/HadesInventoryScreen.h"

#include "Actions/InventoryItemAction.h"
#include "Widgets/InventoryItemMenuWidget.h"

#include "HadesItemMenu.generated.h"

class UCommonButtonBase;


/** One of the inventory screen's item actions, such as Use or Store, for item menus and the screen's default actions. */
UCLASS(meta = (DisplayName = "Hades Item Action"))
class HADES_API UHadesItemAction : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EHadesItemAction Action = EHadesItemAction::Use;

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/**
 * What can be done with one item, opened beside it by the inventory screen. It lists the item actions of its Action Set and Actions.
 * With neither, it lists every Hades item action on the button named for it, such as UseButton, so a menu laid out by hand works as is,
 * and Discard asks first where the menu has a prompt. The screen's action hint says what the highlighted one does.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesItemMenu : public UInventoryItemMenuWidget
{
	GENERATED_BODY()

public:
	UHadesItemMenu(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UInventoryItemMenuWidget Interface
	virtual void GatherActions(TArray<UInventoryItemAction*>& OutActions) const override;
	//~ End UInventoryItemMenuWidget Interface

	/** What each built-in action does, shown for the highlighted button. */
	UPROPERTY(EditDefaultsOnly, Category = "Item Menu")
	TMap<EHadesItemAction, FText> Descriptions;

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

	/** One per Hades item action, listed when the menu has no actions of its own. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesItemAction>> BuiltInActions;
};
