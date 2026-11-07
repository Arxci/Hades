#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"

#include "NativeGameplayTags.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include "InventoryRegistry.h"

class UInventoryContainerLayout;
class UInventoryItemDefinition;

namespace InventoryTests
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_MainHand);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Slot_OffHand);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Durability);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stat_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Herb);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Container_Main);

	/** Transient assets for the specs, kept alive for as long as this exists. */
	struct FTestData
	{
		FTestData();

		UInventoryContainerLayout* Grid4x4 = nullptr;
		UInventoryContainerLayout* Grid4x2 = nullptr;
		UInventoryContainerLayout* Grid2x2 = nullptr;
		UInventoryContainerLayout* Grid1x1 = nullptr;
		/** Equipment slots: main hand, then off hand. */
		UInventoryContainerLayout* Hands = nullptr;
		/** A list holding at most two items. */
		UInventoryContainerLayout* SmallList = nullptr;

		/** 1x1. */
		UInventoryItemDefinition* Stone = nullptr;
		/** 1x1, stacks to 30. */
		UInventoryItemDefinition* Ammo = nullptr;
		/** 1x1, stacks to 5, tagged as a herb. */
		UInventoryItemDefinition* Herb = nullptr;
		/** 1x2, fits either hand, durability 100. */
		UInventoryItemDefinition* Knife = nullptr;
		/** 1x3, main hand only, blocks the off hand. */
		UInventoryItemDefinition* Rifle = nullptr;
		/** 2x2, carries a 2x2 grid. */
		UInventoryItemDefinition* Backpack = nullptr;
		/** 1x1, its socket adds 5 Fire to whatever sits right above it. */
		UInventoryItemDefinition* Torch = nullptr;
		/** 1x1. */
		UInventoryItemDefinition* Medkit = nullptr;

		template <typename T>
		T* Make()
		{
			T* Object = NewObject<T>(GetTransientPackage());
			KeepAlive.Emplace(Object);
			return Object;
		}

	private:
		UInventoryContainerLayout* MakeGrid(FIntPoint Size);

		TArray<TStrongObjectPtr<UObject>> KeepAlive;
	};

	/** The last stack the add touched, or an unset handle when it failed. */
	FInventoryItemHandle AddTestItem(FInventoryRegistry& Registry, UInventoryItemDefinition* Definition, FInventoryContainerHandle Container,
		int32 Count = 1, const TOptional<FInventoryPlacement>& Placement = {});

	/** Names both values in the failure message, which a plain enum comparison wouldn't. */
	inline FString DescribeErrors(const TCHAR* What, EInventoryError Actual, EInventoryError Expected)
	{
		return FString::Printf(TEXT("%s: got %s, expected %s"), What, *UEnum::GetValueAsString(Actual), *UEnum::GetValueAsString(Expected));
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
