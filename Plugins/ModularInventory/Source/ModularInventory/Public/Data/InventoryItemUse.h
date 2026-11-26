#pragma once

#include "CoreMinimal.h"

#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "InventoryItemUse.generated.h"

class UGameplayEffect;


/**
 * Makes an item usable, such as a herb that heals, a ration that feeds, or a scroll read once: an item menu's Use action, a shortcut, or
 * Inventory Component > Use Item uses it. Add it to an item definition's Fragments.
 * Using it applies Effect to the user's ability system, sends Event Tag to the user for an ability to do the rest, then uses up Count Used
 * of the stack. Anything else using it does, listen for the Inventory Component's On Item Used.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Inventory Item Use"))
struct MODULARINVENTORY_API FInventoryItemUse
{
	GENERATED_BODY()

	/** Applied to the user as it's used, such as healing over time. Needs an ability system on the user. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use")
	TSubclassOf<UGameplayEffect> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use", meta = (ClampMin = "0"))
	float EffectLevel = 1.0f;

	/**
	 * Sent to the user as a gameplay event as it's used, with the item's definition as the event's Optional Object, so an ability can do
	 * the rest, such as open a note or play an animation. Optional.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use")
	FGameplayTag EventTag;

	/** How many of the stack using it once uses up. 0 keeps it, such as a map that's looked at, or a reusable tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use", meta = (ClampMin = "0"))
	int32 CountUsed = 1;

	/** Only usable while the user's ability system has these tags, and none of those it rules out, such as not while poisoned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Use")
	FGameplayTagRequirements UserRequirements;
};
