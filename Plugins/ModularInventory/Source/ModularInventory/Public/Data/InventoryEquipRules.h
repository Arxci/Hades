#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "InventoryEquipRules.generated.h"

class UAttributeSet;
class UGameplayAbility;
class UGameplayEffect;


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryAbilityGrant
{
	GENERATED_BODY()

	int32 GetLevel(int32 ItemLevel) const { return bScaleWithItemLevel ? Level + FMath::Max(ItemLevel, 1) - 1 : Level; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> Ability;

	/** The level at item level 1. When scaling, each item level above that adds one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1"))
	int32 Level = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	bool bScaleWithItemLevel = true;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryEffectGrant
{
	GENERATED_BODY()

	float GetLevel(int32 ItemLevel) const { return bScaleWithItemLevel ? Level + FMath::Max(ItemLevel, 1) - 1 : Level; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> Effect;

	/** The level at item level 1. When scaling, each item level above that adds one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "1"))
	float Level = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect")
	bool bScaleWithItemLevel = true;
};


/**
 * Where an item can be equipped and what it grants while it is. Shared between definitions, e.g. one asset for all pistols.
 * The item's level is its Inventory.Item.Level tag value.
 */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryEquipRules : public UDataAsset
{
	GENERATED_BODY()

public:
	bool AllowsSlot(const FGameplayTag& SlotTag) const { return SlotTag.MatchesAny(Slots); }
	bool BlocksSlot(const FGameplayTag& SlotTag) const { return SlotTag.MatchesAny(BlockedSlots); }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	/** Slots the item fits. A slot matches when its tag is one of these or a child of one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slots")
	FGameplayTagContainer Slots;

	/** Slots that must stay empty while this is equipped, like the off hand for a two-handed weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slots")
	FGameplayTagContainer BlockedSlots;

	/** Checked against the owner's ability system tags when equipping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
	FGameplayTagRequirements OwnerRequirements;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granted", meta = (TitleProperty = "Ability"))
	TArray<FInventoryAbilityGrant> Abilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granted", meta = (TitleProperty = "Effect"))
	TArray<FInventoryEffectGrant> Effects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granted")
	TArray<TSubclassOf<UAttributeSet>> AttributeSets;

	/** Loose tags given to the owner while equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Granted")
	FGameplayTagContainer OwnerTags;
};
