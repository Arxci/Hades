#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "InventoryTypes.h"

#include "InventoryEquipmentActorInterface.generated.h"

class UInventoryEquipmentComponent;


UINTERFACE(MinimalAPI, BlueprintType)
class UInventoryEquipmentActorInterface : public UInterface
{
	GENERATED_BODY()
};

/** For actors spawned by equipment pieces that need to know which item they represent. */
class MODULARINVENTORY_API IInventoryEquipmentActorInterface
{
	GENERATED_BODY()

public:
	/** Right after spawning. Read the item's tag values and fragments here, e.g. to drive a Mutable Customizable Object. */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Equipment")
	void OnEquipped(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle Item, FGameplayTag Slot);
	virtual void OnEquipped_Implementation(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle Item, FGameplayTag Slot) {}

	/** Right before the actor is destroyed. */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory|Equipment")
	void OnUnequipped(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle Item, FGameplayTag Slot);
	virtual void OnUnequipped_Implementation(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle Item, FGameplayTag Slot) {}
};
