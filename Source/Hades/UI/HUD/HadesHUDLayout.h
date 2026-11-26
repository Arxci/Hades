#pragma once

#include "CoreMinimal.h"

#include "InventoryTypes.h"
#include "ModularActivatableWidget.h"

#include "HadesHUDLayout.generated.h"

class APawn;
class UCommonActivatableWidget;
class UHadesQuickSlotComponent;
class UWidgetAnimation;

/**
 * The player's HUD, pushed onto the Game layer by the player controller. Escape opens Escape Menu Class on the Menu layer, Inventory opens
 * Inventory Screen Class on the GameMenu layer, and Crafting opens it there on its Crafting Tab.
 * Its input config defaults to Game and Menu: with Game alone, Common UI ignores those actions.
 * Lay a Modular Message Feed for UI.Message.Prompt in it to show what the player looks at, such as an item's icon and name over the key that
 * takes it: the HUD hides while menus pause the game, and the prompt with it.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class HADES_API UHadesHUDLayout : public UModularActivatableWidget
{
	GENERATED_BODY()

public:
	UHadesHUDLayout(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSoftClassPtr<UCommonActivatableWidget> EscapeMenuClass;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSoftClassPtr<UCommonActivatableWidget> InventoryScreenClass;

	/**
	 * The Tab Id of the inventory screen's crafting tab, in its Tab List, which the Crafting action opens it on. Give that tab the Crafting
	 * action as its Shortcut too, so the key also switches to it, and closes the screen from it, while the screen is open.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FName CraftingTab = TEXT("Crafting");

	/** Played from the start whenever a quick slot's key is pressed, such as to show the slots for a moment. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowQuickSlotsAnimation;

private:
	void HandleEscapeAction();
	void HandleInventoryAction();
	void HandleCraftingAction();

	/** Opens Inventory Screen Class, on the tab with the id, or on its items page for None. */
	void OpenInventory(FName TabId);

	/** The HUD is made before the pawn exists, so the quick slots are found once it's possessed. */
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleQuickSlotActivated(int32 SlotIndex, FInventoryItemHandle Item);

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
};
