#pragma once

#include "CoreMinimal.h"

#include "Shortcuts/InventoryShortcutComponent.h"

#include "HadesQuickSlotComponent.generated.h"

class UHadesInventoryComponent;
class UInputAction;
class UInventoryEquipmentComponent;


/**
 * Number-key shortcuts to items the player carries: the plugin's shortcut slots, with the Hades rules on top. A slot takes only things the
 * player can hold in hand or use, and activating it draws its item into a hand, puts it away if it's already in one, or uses it.
 * Each slot's Tag is the input tag that activates it, such as Input.QuickSlot.1. The hero binds each slot to the input action its input
 * config has for that tag, and widgets show the key the player has bound to that action, so rebinding a slot's key changes both.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesQuickSlotComponent : public UInventoryShortcutComponent
{
	GENERATED_BODY()

public:
	UHadesQuickSlotComponent();

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	//~ Begin UInventoryShortcutComponent Interface
	virtual void NativeOnShortcutActivated(int32 SlotIndex, FInventoryItemHandle Item) override;
	virtual bool AcceptsItem_Implementation(int32 SlotIndex, FInventoryItemHandle Item) const override;
	virtual const UInputAction* GetSlotInputAction_Implementation(int32 SlotIndex) const override;
	//~ End UInventoryShortcutComponent Interface

private:
	/** What's in hand changes without the inventory changing, as it stays in the inventory, so the slots show it changed. */
	UFUNCTION()
	void HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag Slot);

	UHadesInventoryComponent* GetHadesInventory() const;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryEquipmentComponent> Equipment;
};
