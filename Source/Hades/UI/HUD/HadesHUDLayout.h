#pragma once

#include "CoreMinimal.h"

#include "InventoryTypes.h"
#include "ModularActivatableWidget.h"

#include "HadesHUDLayout.generated.h"

class APawn;
class UCommonActivatableWidget;
class UHadesQuickSlotComponent;
class UInputAction;
class UWidgetAnimation;

/**
 * The player's HUD, pushed onto the Game layer by the player controller. Pause Input Action opens Escape Menu Class on the Menu layer,
 * Inventory Input Action opens Inventory Screen Class on the GameMenu layer, and Crafting Input Action opens it there on its Crafting Tab.
 * They only work while the HUD is the top of what's open, as in gameplay: each screen they open binds its own keys to close it, in its
 * Close Input Actions, and the Crafting tab's in its Shortcut Input Action.
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

	/** The pause menu, which Pause Input Action opens. Give it a Close Input Action that's only the gamepad's Start, such as IA_UI_Resume. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSoftClassPtr<UCommonActivatableWidget> EscapeMenuClass;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSoftClassPtr<UCommonActivatableWidget> InventoryScreenClass;

	/**
	 * The Tab Id of the inventory screen's crafting tab, in its Tab List, which Crafting Input Action opens it on. Give that tab the same
	 * input action as its Shortcut Input Action, so the key also switches to it, and closes the screen from it, while the screen is open.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FName CraftingTab = TEXT("Crafting");

	/** Played from the start whenever a quick slot's key is pressed, such as to show the slots for a moment. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowQuickSlotsAnimation;

	// Input actions, mapped in an Input Mapping Context in the player controller's Menu Mapping Contexts so they're added and can be rebound.
	/** Opens Escape Menu Class, such as IA_UI_Pause on Escape and the gamepad's Start. Escape is Back in menus, so menus don't use this one. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD|Input")
	TObjectPtr<const UInputAction> PauseInputAction;

	/** Opens Inventory Screen Class. Give the inventory screen the same action in its Close Input Actions, so the key closes it too. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD|Input")
	TObjectPtr<const UInputAction> InventoryInputAction;

	/** Opens Inventory Screen Class on Crafting Tab. Give that tab the same action as its Shortcut Input Action. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD|Input")
	TObjectPtr<const UInputAction> CraftingInputAction;

private:
	void HandlePauseAction();
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
