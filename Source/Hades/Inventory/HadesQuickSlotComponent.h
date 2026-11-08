#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "InventoryTypes.h"

#include "HadesQuickSlotComponent.generated.h"

class UHadesInventoryComponent;
class UInventoryComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHadesQuickSlotsChangedSignature);


/**
 * Number-key shortcuts to items the player carries. A slot follows its item wherever it moves, and empties when the item leaves the inventory.
 * Activating a slot draws its item into a hand, puts it away if it's already in one, or uses it.
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

	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	int32 GetNumSlots() const { return NumSlots; }

	/** Something to hold or to use. */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	bool CanAssign(FInventoryItemHandle Item) const;

	/** The slots changed, or what's in hand did. */
	UPROPERTY(BlueprintAssignable, Category = "Quick Slots")
	FHadesQuickSlotsChangedSignature OnChanged;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/** The hero binds Input.QuickSlot.1 to 4, so more than four need more input. */
	UPROPERTY(EditAnywhere, Category = "Quick Slots", meta = (ClampMin = "1"))
	int32 NumSlots = 4;

private:
	UFUNCTION()
	void HandleInventoryChanged(UInventoryComponent* ChangedInventory);

	UPROPERTY(Transient)
	TObjectPtr<UHadesInventoryComponent> Inventory;

	TArray<FInventoryItemHandle> Slots;
};
