#pragma once

#include "CoreMinimal.h"

#include "Misc/EnumRange.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"

#include "InventoryTypes.h"

#include "HadesInventoryScreen.generated.h"

class UHadesInventoryComponent;
class UHadesItemMenu;
class UInventoryGridWidget;
class UInventoryListWidget;
class UInventoryUISubsystem;


UENUM(BlueprintType)
enum class EHadesItemAction : uint8
{
	Use,
	Equip,
	Unequip,
	Combine,
	Discard,
	/** Into the item box. */
	Store,
	/** Out of the item box. */
	Take
};
ENUM_RANGE_BY_FIRST_AND_LAST(EHadesItemAction, EHadesItemAction::Use, EHadesItemAction::Take);


/**
 * The player's inventory, pushed onto the GameMenu layer by the HUD, or by an item box when the screen has an Item Box List.
 * The world stops while it's open. Right-click an item, or press Item Menu on it, for what can be done with it.
 * Rotate Item and Split Stack work on the held item, Organize repacks the pockets, and Back puts a held item down before it closes the screen.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API UHadesInventoryScreen : public UHadesActivatableWidget
{
	GENERATED_BODY()

public:
	UHadesInventoryScreen(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanRunItemAction(EHadesItemAction Action, FInventoryItemHandle Item) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RunItemAction(EHadesItemAction Action, FInventoryItemHandle Item);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget Interface

	/** Pushed onto the Modal layer for the item the player right-clicks or presses Item Menu on. */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UHadesItemMenu> ItemMenuClass;

	/** Its first cell takes gamepad focus when the screen opens, unless the Blueprint implements Get Desired Focus Target. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryGridWidget> MainGrid;

	/** Shows the item box, which turns on Store and Take. Leave it out of the plain inventory screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UInventoryListWidget> ItemBoxList;

private:
	UFUNCTION()
	void OpenItemMenu(FInventoryItemHandle Item);

	UFUNCTION()
	void HandleItemChosen(FInventoryItemHandle Item);

	UInventoryUISubsystem* GetInteraction() const;
	UHadesInventoryComponent* GetPlayerInventory() const;
	FInventoryContainerHandle GetItemBox() const;

	void HandleRotateAction();
	void HandleSplitAction();
	void HandleItemMenuAction();
	void HandleOrganizeAction();
	void HandleCloseAction();

	/** The first of two items to combine, while the player chooses the second. */
	FInventoryItemHandle CombineItem;
};
