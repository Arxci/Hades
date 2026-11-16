#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"

#include "HadesQuickSlotComponent.generated.h"

class UHadesInventoryComponent;
class UInputAction;
class UInventoryComponent;
class UInventoryEquipmentComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHadesQuickSlotsChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesQuickSlotActivatedSignature, int32, SlotIndex);


/**
 * Number-key shortcuts to items the player carries. A slot follows its item wherever it moves, and empties when the item leaves the inventory.
 * Activating a slot draws its item into a hand, puts it away if it's already in one, or uses it.
 * There's a slot per tag in Slot Inputs. The hero binds each slot to the input action its input config has for that tag, and widgets show
 * the key the player has bound to that action, so rebinding a slot's key changes both.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHadesQuickSlotComponent();

	/** Moves the item here from any other slot. */
	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void AssignItem(int32 SlotIndex, FInventoryItemHandle Item);

	/** For new finds: puts something to hold in the first empty slot, unless it has a slot already. */
	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void AssignToFreeSlot(FInventoryItemHandle Item);

	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void ActivateSlot(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	FInventoryItemHandle GetSlotItem(int32 SlotIndex) const;

	/** The slot the item is in, or -1. */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	int32 FindSlot(FInventoryItemHandle Item) const;

	/** The input tag that activates the slot, such as Input.QuickSlot.1. */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	FGameplayTag GetSlotInputTag(int32 SlotIndex) const;

	/**
	 * The input action that activates the slot: the one the hero's input config has for its tag. Give it to a Hades Input Key Widget to
	 * show the key the player has bound to it. Null without a hero or an action for the tag.
	 */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	const UInputAction* GetSlotInputAction(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	int32 GetNumSlots() const { return SlotInputs.Num(); }

	/** Something to hold or to use. */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	bool CanAssign(FInventoryItemHandle Item) const;

	/** The slots changed, or what's in hand did. */
	UPROPERTY(BlueprintAssignable, Category = "Quick Slots")
	FHadesQuickSlotsChangedSignature OnChanged;

	/** The player pressed a slot's key, even with nothing on it, such as to show the slots for a moment. */
	UPROPERTY(BlueprintAssignable, Category = "Quick Slots")
	FHadesQuickSlotActivatedSignature OnSlotActivated;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/**
	 * The input tag that activates each slot, slot 1 first: one slot per tag. Each needs an input action for it in the hero's input
	 * config, mapped to a key in a mapping context. Input.QuickSlot.1 to 4 by default.
	 */
	UPROPERTY(EditAnywhere, Category = "Quick Slots", meta = (Categories = "Input"))
	TArray<FGameplayTag> SlotInputs;

private:
	UFUNCTION()
	void HandleInventoryChanged(UInventoryComponent* ChangedInventory);

	UFUNCTION()
	void HandleEquipmentChanged(FInventoryItemHandle Item, FGameplayTag Slot);

	UPROPERTY(Transient)
	TObjectPtr<UHadesInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryEquipmentComponent> Equipment;

	TArray<FInventoryItemHandle> Slots;
};
