#include "InventoryFunctionLibrary.h"

#include "Abilities/GameplayAbility.h"

#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventorySubsystem.h"
#include "Loot/InventoryLootTable.h"

namespace
{
	EInventoryFragmentResult CopyFragment(const FInstancedStruct* Fragment, const FStructProperty* TargetProperty, void* TargetAddress)
	{
		if (!Fragment || !TargetProperty || !TargetAddress || Fragment->GetScriptStruct() != TargetProperty->Struct)
		{
			return EInventoryFragmentResult::NotFound;
		}

		TargetProperty->Struct->CopyScriptStruct(TargetAddress, Fragment->GetMemory());
		return EInventoryFragmentResult::Found;
	}

	FInstancedStruct MakeFragment(const FStructProperty* SourceProperty, const void* SourceAddress)
	{
		FInstancedStruct Fragment;
		if (SourceProperty && SourceAddress)
		{
			Fragment.InitializeAs(SourceProperty->Struct, static_cast<const uint8*>(SourceAddress));
		}
		return Fragment;
	}
}

// Custom thunks read the wildcard pin themselves, in the order the parameters are declared.
DEFINE_FUNCTION(UInventoryFunctionLibrary::execGetItemFragment)
{
	P_GET_ENUM_REF(EInventoryFragmentResult, Result);
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT(FInventoryItemHandle, Item);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* FragmentProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* FragmentAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	P_NATIVE_BEGIN;
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(WorldContextObject);
	const FInstancedStruct* Fragment = Subsystem && FragmentProperty ? Subsystem->GetRegistry().FindItemFragment(Item, FragmentProperty->Struct) : nullptr;
	Result = CopyFragment(Fragment, FragmentProperty, FragmentAddress);
	P_NATIVE_END;
}

DEFINE_FUNCTION(UInventoryFunctionLibrary::execSetItemFragment)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT(FInventoryItemHandle, Item);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* FragmentProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const void* FragmentAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	P_NATIVE_BEGIN;
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(WorldContextObject);
	const FInstancedStruct NewFragment = MakeFragment(FragmentProperty, FragmentAddress);
	*static_cast<bool*>(RESULT_PARAM) = Subsystem && Subsystem->GetRegistry().SetItemFragment(Item, NewFragment) == EInventoryError::None;
	P_NATIVE_END;
}

DEFINE_FUNCTION(UInventoryFunctionLibrary::execGetContainerFragment)
{
	P_GET_ENUM_REF(EInventoryFragmentResult, Result);
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT(FInventoryContainerHandle, Container);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* FragmentProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* FragmentAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	P_NATIVE_BEGIN;
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(WorldContextObject);
	const FInstancedStruct* Fragment = Subsystem && FragmentProperty ? Subsystem->GetRegistry().FindContainerFragment(Container, FragmentProperty->Struct) : nullptr;
	Result = CopyFragment(Fragment, FragmentProperty, FragmentAddress);
	P_NATIVE_END;
}

DEFINE_FUNCTION(UInventoryFunctionLibrary::execSetContainerFragment)
{
	P_GET_OBJECT(UObject, WorldContextObject);
	P_GET_STRUCT(FInventoryContainerHandle, Container);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* FragmentProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const void* FragmentAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	P_NATIVE_BEGIN;
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(WorldContextObject);
	const FInstancedStruct NewFragment = MakeFragment(FragmentProperty, FragmentAddress);
	*static_cast<bool*>(RESULT_PARAM) = Subsystem && Subsystem->GetRegistry().SetContainerFragment(Container, NewFragment) == EInventoryError::None;
	P_NATIVE_END;
}

DEFINE_FUNCTION(UInventoryFunctionLibrary::execGetDefinitionFragment)
{
	P_GET_ENUM_REF(EInventoryFragmentResult, Result);
	P_GET_OBJECT(UInventoryItemDefinition, Definition);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FStructProperty* FragmentProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* FragmentAddress = Stack.MostRecentPropertyAddress;

	P_FINISH;

	P_NATIVE_BEGIN;
	const FInstancedStruct* Fragment = Definition && FragmentProperty ? Definition->FindFragment(FragmentProperty->Struct) : nullptr;
	Result = CopyFragment(Fragment, FragmentProperty, FragmentAddress);
	P_NATIVE_END;
}

FInventoryItemHandle UInventoryFunctionLibrary::GetItemFromAbility(const UGameplayAbility* Ability)
{
	const UInventoryEquipmentComponent* Equipment = Ability ? Cast<UInventoryEquipmentComponent>(Ability->GetCurrentSourceObject()) : nullptr;
	return Equipment ? Equipment->GetItemForAbility(Ability->GetCurrentAbilitySpecHandle()) : FInventoryItemHandle();
}

TArray<FInventoryItemSpec> UInventoryFunctionLibrary::RollLootTable(const UInventoryLootTable* LootTable, const FGameplayTagContainer& ContextTags, int32 Seed)
{
	TArray<FInventoryItemSpec> Items;
	if (LootTable)
	{
		FRandomStream Stream(Seed >= 0 ? Seed : FMath::Rand());
		LootTable->Roll(Stream, ContextTags, Items);
	}
	return Items;
}
