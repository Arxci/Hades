#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "InventoryTypes.h"

#include "InventoryFunctionLibrary.generated.h"

class UGameplayAbility;
class UInventoryItemDefinition;
class UInventoryLootTable;


UENUM(BlueprintType)
enum class EInventoryFragmentResult : uint8
{
	Found,
	NotFound
};


UCLASS()
class MODULARINVENTORY_API UInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Handles
	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "Equal (Inventory Item Handle)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_InventoryItemHandle(FInventoryItemHandle A, FInventoryItemHandle B) { return A == B; }

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "Not Equal (Inventory Item Handle)", CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEqual_InventoryItemHandle(FInventoryItemHandle A, FInventoryItemHandle B) { return A != B; }

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "Equal (Inventory Container Handle)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_InventoryContainerHandle(FInventoryContainerHandle A, FInventoryContainerHandle B) { return A == B; }

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "Not Equal (Inventory Container Handle)", CompactNodeTitle = "!=", Keywords = "!= not equal"))
	static bool NotEqual_InventoryContainerHandle(FInventoryContainerHandle A, FInventoryContainerHandle B) { return A != B; }

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "To String (Inventory Item Handle)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_InventoryItemHandleToString(FInventoryItemHandle Item) { return Item.ToString(); }

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DisplayName = "To String (Inventory Container Handle)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_InventoryContainerHandleToString(FInventoryContainerHandle Container) { return Container.ToString(); }

	// Placements
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static FInventoryPlacement MakeCellPlacement(FIntPoint Cell, bool bRotated = false) { return FInventoryPlacement::AtCell(Cell, bRotated); }

	/** A slot index, or a position in a list. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static FInventoryPlacement MakeIndexPlacement(int32 Index) { return FInventoryPlacement::AtIndex(Index); }

	// Fragments. The struct type comes from whatever is connected to the fragment pin.
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Inventory|Fragments", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "OutFragment", ExpandEnumAsExecs = "Result"))
	static void GetItemFragment(EInventoryFragmentResult& Result, const UObject* WorldContextObject, FInventoryItemHandle Item, int32& OutFragment);
	DECLARE_FUNCTION(execGetItemFragment);

	/** Replaces the item's fragment of the same type, or adds it. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Inventory|Fragments", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "Fragment"))
	static bool SetItemFragment(const UObject* WorldContextObject, FInventoryItemHandle Item, const int32& Fragment);
	DECLARE_FUNCTION(execSetItemFragment);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Inventory|Fragments", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "OutFragment", ExpandEnumAsExecs = "Result"))
	static void GetContainerFragment(EInventoryFragmentResult& Result, const UObject* WorldContextObject, FInventoryContainerHandle Container, int32& OutFragment);
	DECLARE_FUNCTION(execGetContainerFragment);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Inventory|Fragments", meta = (WorldContext = "WorldContextObject", CustomStructureParam = "Fragment"))
	static bool SetContainerFragment(const UObject* WorldContextObject, FInventoryContainerHandle Container, const int32& Fragment);
	DECLARE_FUNCTION(execSetContainerFragment);

	/** Reads one of the definition's static fragments. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Inventory|Fragments", meta = (CustomStructureParam = "OutFragment", ExpandEnumAsExecs = "Result"))
	static void GetDefinitionFragment(EInventoryFragmentResult& Result, const UInventoryItemDefinition* Definition, int32& OutFragment);
	DECLARE_FUNCTION(execGetDefinitionFragment);

	// Abilities
	/** The equipped item that granted this ability instance, if equipment granted it. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	static FInventoryItemHandle GetItemFromAbility(const UGameplayAbility* Ability);

	// Loot
	/** A negative seed picks a random one. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Loot")
	static TArray<FInventoryItemSpec> RollLootTable(const UInventoryLootTable* LootTable, const FGameplayTagContainer& ContextTags, int32 Seed = -1);
};
