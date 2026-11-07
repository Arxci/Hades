#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"
#include "UObject/ObjectKey.h"

#include "InventoryTypes.h"

#include "InventoryLootTable.generated.h"

class UInventoryItemDefinition;
class UInventoryLootTable;


UENUM(BlueprintType)
enum class EInventoryLootEntryType : uint8
{
	Item,
	/** Rolls another table. */
	Table,
	/** Drops nothing; its weight is the chance of an empty roll. */
	Nothing
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryTagValueRange
{
	GENERATED_BODY()

	float Roll(FRandomStream& Stream) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	float Max = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	bool bWholeNumber = true;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	EInventoryLootEntryType Type = EInventoryLootEntryType::Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (EditCondition = "Type == EInventoryLootEntryType::Item", EditConditionHides))
	TObjectPtr<UInventoryItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (EditCondition = "Type == EInventoryLootEntryType::Table", EditConditionHides))
	TObjectPtr<UInventoryLootTable> Table;

	/** Item: how many. Table: how many times to roll it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;

	/** Chance relative to the pool's other entries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	float Weight = 1.0f;

	/** Tested against the roll's context tags. Empty always passes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FGameplayTagQuery Condition;

	/** Random starting values, e.g. durability between 20 and 60. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (TitleProperty = "Tag", EditCondition = "Type == EInventoryLootEntryType::Item", EditConditionHides))
	TArray<FInventoryTagValueRange> TagValues;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryLootPool
{
	GENERATED_BODY()

	/** Each roll picks one entry by weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 MinRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0"))
	int32 MaxRolls = 1;

	/** Tested against the roll's context tags. Empty always passes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FGameplayTagQuery Condition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TArray<FInventoryLootEntry> Entries;
};


USTRUCT()
struct FInventoryInjectedLootPools
{
	GENERATED_BODY()

	FObjectKey Source;

	UPROPERTY()
	TArray<FInventoryLootPool> Pools;
};


/** Random item generation. Each pool rolls on its own, and game features can add pools while they are active. */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Appends what the table produced. Pool and entry conditions are tested against the context tags. */
	void Roll(FRandomStream& Stream, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemSpec>& OutItems) const;

	/** Added pools roll after the table's own until removed by the same source. */
	void AddInjectedPools(const UObject* Source, const TArray<FInventoryLootPool>& InPools);
	void RemoveInjectedPools(const UObject* Source);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TArray<FInventoryLootPool> Pools;

private:
	void RollPools(const TArray<FInventoryLootPool>& InPools, FRandomStream& Stream, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemSpec>& OutItems, int32 Depth) const;
	void RollTable(FRandomStream& Stream, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemSpec>& OutItems, int32 Depth) const;

	UPROPERTY(Transient)
	TArray<FInventoryInjectedLootPools> InjectedPools;
};
