#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"

#include "HadesHUDLayout.generated.h"

class APawn;
class UCommonActivatableWidget;
class UHadesQuickSlotComponent;
class UWidgetAnimation;

/**
 * The player's HUD, pushed onto the Game layer by the player controller. Escape opens EscapeMenuClass on the Menu layer, and Inventory opens InventoryScreenClass on the GameMenu layer.
 * Its input config defaults to Game and Menu: with Game alone, Common UI ignores those two actions.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class HADES_API UHadesHUDLayout : public UHadesActivatableWidget
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

	/** Played from the start whenever a quick slot's key is pressed, such as to show the slots for a moment, as the RE games do. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowQuickSlotsAnimation;

private:
	void HandleEscapeAction();
	void HandleInventoryAction();

	/** The HUD is made before the pawn exists, so the quick slots are found once it's possessed. */
	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleQuickSlotActivated(int32 SlotIndex);

	TWeakObjectPtr<UHadesQuickSlotComponent> QuickSlots;
};
