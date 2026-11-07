#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"

#include "InventoryStackRules.generated.h"


/**
 * How items of a kind stack. Shared between item definitions, e.g. one asset for all ammo.
 * Two stacks only merge when they have the same definition and identical instance state: tags, tag values and fragments.
 */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryStackRules : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking", meta = (ClampMin = "1"))
	int32 MaxStackSize = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stacking")
	bool bCanSplit = true;
};
