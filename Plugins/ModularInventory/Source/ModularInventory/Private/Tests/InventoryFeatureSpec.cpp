#include "Tests/InventoryTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryItemDefinition.h"
#include "Loot/InventoryLootTable.h"

using namespace InventoryTests;

BEGIN_DEFINE_SPEC(FInventoryFeatureSpec, "ModularInventory.Features", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FTestData> Data;
	TUniquePtr<FInventoryRegistry> Registry;
	FInventoryContainerHandle Grid;
	FInventoryContainerHandle Hands;

	int32 Count(UInventoryItemDefinition* Definition) const
	{
		FInventoryItemQuery Query;
		Query.Definitions.Add(Definition);
		return Registry->CountItems(MakeArrayView(&Grid, 1), Query, true);
	}

	UInventoryRecipe* MakeMedkitRecipe(int32 HerbsNeeded) const
	{
		UInventoryRecipe* Recipe = Data->Make<UInventoryRecipe>();
		FInventoryRecipeIngredient& Ingredient = Recipe->Ingredients.AddDefaulted_GetRef();
		Ingredient.Query.Definitions.Add(Data->Herb);
		Ingredient.Count = HerbsNeeded;
		Recipe->Outputs.Emplace(Data->Medkit, 1);
		return Recipe;
	}

	static void AddLootEntry(FInventoryLootPool& Pool, UInventoryItemDefinition* Item, int32 MinCount = 1, int32 MaxCount = 1)
	{
		FInventoryLootEntry& Entry = Pool.Entries.AddDefaulted_GetRef();
		Entry.Item = Item;
		Entry.MinCount = MinCount;
		Entry.MaxCount = MaxCount;
	}
END_DEFINE_SPEC(FInventoryFeatureSpec)

void FInventoryFeatureSpec::Define()
{
	BeforeEach([this]()
	{
		Data = MakeUnique<FTestData>();
		Registry = MakeUnique<FInventoryRegistry>();
		Grid = Registry->CreateContainer(Data->Grid4x4, FGameplayTag(), nullptr);
		Hands = Registry->CreateContainer(Data->Hands, FGameplayTag(), nullptr);
	});

	AfterEach([this]()
	{
		Registry.Reset();
		Data.Reset();
	});

	Describe("Equipment", [this]()
	{
		It("only accepts items whose rules allow the slot", [this]()
		{
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Grid);
			const EInventoryError Error = Registry->EquipItem(Stone, Hands, Slot_MainHand);
			TestTrue(*DescribeErrors(TEXT("Equipping a stone"), Error, EInventoryError::NotEquippable), Error == EInventoryError::NotEquippable);

			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			TestTrue(TEXT("Knife equipped"), Registry->EquipItem(Knife, Hands, Slot_MainHand) == EInventoryError::None);
			TestTrue(TEXT("Knife in the main hand"), Registry->GetItemAt(Hands, FInventoryPlacement::AtIndex(0)) == Knife);
		});

		It("keeps the off hand free for two-handed items", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			const FInventoryItemHandle Rifle = AddTestItem(*Registry, Data->Rifle, Grid);

			Registry->EquipItem(Knife, Hands, Slot_OffHand);
			const EInventoryError RifleBlocked = Registry->EquipItem(Rifle, Hands, Slot_MainHand);
			TestTrue(*DescribeErrors(TEXT("Rifle with a full off hand"), RifleBlocked, EInventoryError::NotEquippable), RifleBlocked == EInventoryError::NotEquippable);

			TestTrue(TEXT("Knife unequipped"), Registry->UnequipItem(Knife, Grid) == EInventoryError::None);
			TestTrue(TEXT("Rifle equipped"), Registry->EquipItem(Rifle, Hands, Slot_MainHand) == EInventoryError::None);

			const EInventoryError KnifeBlocked = Registry->EquipItem(Knife, Hands, Slot_OffHand);
			TestTrue(*DescribeErrors(TEXT("Knife beside the rifle"), KnifeBlocked, EInventoryError::NotEquippable), KnifeBlocked == EInventoryError::NotEquippable);
		});
	});

	Describe("Batches", [this]()
	{
		It("undo every operation when one fails", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));

			FInventoryOperation AddStone = FInventoryOperation::MakeAdd(FInventoryItemSpec(Data->Stone), Grid);
			AddStone.bAutoPlace = false;
			AddStone.Placement = FInventoryPlacement::AtCell(FIntPoint(1, 0));

			const TArray<FInventoryOperation> Operations =
			{
				FInventoryOperation::MakeMoveTo(Knife, Grid, FInventoryPlacement::AtCell(FIntPoint(1, 0))),
				AddStone
			};
			const FInventoryBatchResult Result = Registry->ExecuteBatch(Operations);

			TestTrue(*DescribeErrors(TEXT("Adding onto the moved knife"), Result.Error, EInventoryError::Occupied), Result.Error == EInventoryError::Occupied);
			TestEqual(TEXT("Failed operation"), Result.FailedOperation, 1);
			TestTrue(TEXT("Knife back where it started"), Registry->GetItemPlacement(Knife).Cell == FIntPoint(0, 0));
			TestEqual(TEXT("No stone"), Count(Data->Stone), 0);
		});

		It("bring destroyed items back with their contents and handles", [this]()
		{
			const FInventoryItemHandle Backpack = AddTestItem(*Registry, Data->Backpack, Grid);
			const FInventoryContainerHandle Storage = Registry->FindItemContainer(Backpack, Container_Main);
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Storage);

			const TArray<FInventoryOperation> Operations = { FInventoryOperation::MakeRemove(Backpack), FInventoryOperation::MakeRotate(FInventoryItemHandle()) };
			Registry->ExecuteBatch(Operations);

			TestTrue(TEXT("Backpack restored"), Registry->IsValid(Backpack));
			TestTrue(TEXT("Storage restored"), Registry->IsValid(Storage));
			TestTrue(TEXT("Stone restored inside it"), Registry->IsValid(Stone) && Registry->FindItem(Stone)->Container == Storage);
		});

		It("only undo the inner part of a nested failure", [this]()
		{
			const EInventoryError Error = Registry->RunTransaction([this]()
			{
				AddTestItem(*Registry, Data->Stone, Grid);
				Registry->RunTransaction([this]()
				{
					AddTestItem(*Registry, Data->Stone, Grid);
					return EInventoryError::NoSpace;
				});
				return EInventoryError::None;
			});

			TestTrue(TEXT("Outer transaction committed"), Error == EInventoryError::None);
			TestEqual(TEXT("Only the outer stone exists"), Count(Data->Stone), 1);
		});

		It("report changes once, after committing", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));

			int32 Broadcasts = 0;
			const FDelegateHandle Listener = Registry->OnChanged.AddLambda([&Broadcasts](const FInventoryChangeSet&) { ++Broadcasts; });

			const TArray<FInventoryOperation> Moves =
			{
				FInventoryOperation::MakeMoveTo(Knife, Grid, FInventoryPlacement::AtCell(FIntPoint(1, 0))),
				FInventoryOperation::MakeMoveTo(Knife, Grid, FInventoryPlacement::AtCell(FIntPoint(2, 0)))
			};
			Registry->ExecuteBatch(Moves);
			TestEqual(TEXT("One broadcast for the batch"), Broadcasts, 1);

			const TArray<FInventoryOperation> Failing = { FInventoryOperation::MakeRotate(FInventoryItemHandle()) };
			Registry->ExecuteBatch(Failing);
			TestEqual(TEXT("None for a failed batch"), Broadcasts, 1);

			Registry->OnChanged.Remove(Listener);
		});

		It("dry run to the real result without changing anything", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));

			int32 Broadcasts = 0;
			const FDelegateHandle Listener = Registry->OnChanged.AddLambda([&Broadcasts](const FInventoryChangeSet&) { ++Broadcasts; });

			const EInventoryError Error = Registry->DryRun([this, Knife]()
			{
				return Registry->MoveItem(Knife, Grid, FInventoryPlacement::AtCell(FIntPoint(1, 0)));
			});

			TestTrue(TEXT("The move would succeed"), Error == EInventoryError::None);
			TestTrue(TEXT("The knife didn't move"), Registry->GetItemPlacement(Knife).Cell == FIntPoint(0, 0));
			TestEqual(TEXT("Nobody heard about it"), Broadcasts, 0);

			Registry->OnChanged.Remove(Listener);
		});
	});

	Describe("Sockets", [this]()
	{
		It("buff the item in the socket's cell", [this]()
		{
			AddTestItem(*Registry, Data->Torch, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 1)));
			const FInventoryItemHandle Above = AddTestItem(*Registry, Data->Stone, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 0)));
			const FInventoryItemHandle Beside = AddTestItem(*Registry, Data->Stone, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(2, 0)));

			TestEqual(TEXT("Stone above the torch"), Registry->GetEffectiveTagValue(Above, Stat_Fire), 5.0f);
			TestEqual(TEXT("Its own value is untouched"), Registry->GetTagValue(Above, Stat_Fire), 0.0f);
			TestEqual(TEXT("Stone beside it"), Registry->GetEffectiveTagValue(Beside, Stat_Fire), 0.0f);
			TestEqual(TEXT("One source"), Registry->GetSocketSources(Above).Num(), 1);
		});

		It("turn with their item", [this]()
		{
			AddTestItem(*Registry, Data->Torch, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 1), true));
			const FInventoryItemHandle Right = AddTestItem(*Registry, Data->Stone, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(2, 1)));
			const FInventoryItemHandle Above = AddTestItem(*Registry, Data->Stone, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 0)));

			TestEqual(TEXT("Stone right of the turned torch"), Registry->GetEffectiveTagValue(Right, Stat_Fire), 5.0f);
			TestEqual(TEXT("Stone above it"), Registry->GetEffectiveTagValue(Above, Stat_Fire), 0.0f);
		});
	});

	Describe("Loot", [this]()
	{
		It("rolls the same items for the same seed", [this]()
		{
			UInventoryLootTable* Table = Data->Make<UInventoryLootTable>();
			FInventoryLootPool& Pool = Table->Pools.AddDefaulted_GetRef();
			Pool.MinRolls = 3;
			Pool.MaxRolls = 3;
			AddLootEntry(Pool, Data->Ammo, 1, 10);
			AddLootEntry(Pool, Data->Stone);

			TArray<FInventoryItemSpec> First;
			TArray<FInventoryItemSpec> Second;
			FRandomStream FirstStream(42);
			FRandomStream SecondStream(42);
			Table->Roll(FirstStream, FGameplayTagContainer(), First);
			Table->Roll(SecondStream, FGameplayTagContainer(), Second);

			bool bSame = First.Num() == 3 && Second.Num() == 3;
			for (int32 Index = 0; bSame && Index < First.Num(); ++Index)
			{
				bSame = First[Index].Definition == Second[Index].Definition && First[Index].Count == Second[Index].Count;
			}
			TestTrue(TEXT("Same seed, same loot"), bSame);
		});

		It("rolls injected pools until they are removed", [this]()
		{
			UInventoryLootTable* Table = Data->Make<UInventoryLootTable>();
			TArray<FInventoryLootPool> Injected;
			AddLootEntry(Injected.AddDefaulted_GetRef(), Data->Stone);

			FRandomStream Stream(7);
			TArray<FInventoryItemSpec> Rolled;
			Table->AddInjectedPools(Data->Stone, Injected);
			Table->Roll(Stream, FGameplayTagContainer(), Rolled);
			TestEqual(TEXT("Injected pool rolled"), Rolled.Num(), 1);

			Rolled.Reset();
			Table->RemoveInjectedPools(Data->Stone);
			Table->Roll(Stream, FGameplayTagContainer(), Rolled);
			TestEqual(TEXT("Nothing after removal"), Rolled.Num(), 0);
		});

		It("skips entries whose condition fails", [this]()
		{
			UInventoryLootTable* Table = Data->Make<UInventoryLootTable>();
			FInventoryLootPool& Pool = Table->Pools.AddDefaulted_GetRef();
			AddLootEntry(Pool, Data->Medkit);
			Pool.Entries[0].Condition = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(Type_Herb.GetTag()));

			FRandomStream Stream(7);
			TArray<FInventoryItemSpec> Rolled;
			Table->Roll(Stream, FGameplayTagContainer(), Rolled);
			TestEqual(TEXT("Without the context tag"), Rolled.Num(), 0);

			Table->Roll(Stream, FGameplayTagContainer(Type_Herb.GetTag()), Rolled);
			TestEqual(TEXT("With the context tag"), Rolled.Num(), 1);
		});
	});

	Describe("Crafting", [this]()
	{
		It("consumes ingredients and adds the results", [this]()
		{
			AddTestItem(*Registry, Data->Herb, Grid, 3);
			FRandomStream Stream(1);

			const EInventoryError Error = Registry->Craft(*MakeMedkitRecipe(2), MakeArrayView(&Grid, 1), MakeArrayView(&Grid, 1), FGameplayTagContainer(), 1, Stream);
			TestTrue(*DescribeErrors(TEXT("Crafting"), Error, EInventoryError::None), Error == EInventoryError::None);
			TestEqual(TEXT("Herbs left"), Count(Data->Herb), 1);
			TestEqual(TEXT("Medkits made"), Count(Data->Medkit), 1);
		});

		It("keeps the ingredients when the results don't fit", [this]()
		{
			AddTestItem(*Registry, Data->Herb, Grid, 2);
			const FInventoryContainerHandle Full = Registry->CreateContainer(Data->Grid1x1, FGameplayTag(), nullptr);
			AddTestItem(*Registry, Data->Stone, Full);
			FRandomStream Stream(1);

			const EInventoryError Error = Registry->Craft(*MakeMedkitRecipe(2), MakeArrayView(&Grid, 1), MakeArrayView(&Full, 1), FGameplayTagContainer(), 1, Stream);
			TestTrue(*DescribeErrors(TEXT("Crafting into a full container"), Error, EInventoryError::NoSpace), Error == EInventoryError::NoSpace);
			TestEqual(TEXT("Herbs kept"), Count(Data->Herb), 2);
		});

		It("never counts one item toward two ingredients", [this]()
		{
			UInventoryRecipe* Recipe = MakeMedkitRecipe(2);
			FInventoryRecipeIngredient& Second = Recipe->Ingredients.AddDefaulted_GetRef();
			Second.Query.Definitions.Add(Data->Herb);
			Second.Count = 2;

			AddTestItem(*Registry, Data->Herb, Grid, 3);
			const EInventoryError Short = Registry->CanCraft(*Recipe, MakeArrayView(&Grid, 1), FGameplayTagContainer());
			TestTrue(*DescribeErrors(TEXT("Three herbs for four"), Short, EInventoryError::MissingIngredients), Short == EInventoryError::MissingIngredients);

			AddTestItem(*Registry, Data->Herb, Grid, 1);
			TestTrue(TEXT("Four herbs for four"), Registry->CanCraft(*Recipe, MakeArrayView(&Grid, 1), FGameplayTagContainer()) == EInventoryError::None);
		});

		It("keeps tools", [this]()
		{
			UInventoryRecipe* Recipe = MakeMedkitRecipe(1);
			FInventoryRecipeIngredient& Tool = Recipe->Ingredients.AddDefaulted_GetRef();
			Tool.Query.Definitions.Add(Data->Knife);
			Tool.bConsume = false;

			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			AddTestItem(*Registry, Data->Herb, Grid, 1);
			FRandomStream Stream(1);

			TestTrue(TEXT("Crafted"), Registry->Craft(*Recipe, MakeArrayView(&Grid, 1), MakeArrayView(&Grid, 1), FGameplayTagContainer(), 1, Stream) == EInventoryError::None);
			TestTrue(TEXT("Knife kept"), Registry->IsValid(Knife));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
