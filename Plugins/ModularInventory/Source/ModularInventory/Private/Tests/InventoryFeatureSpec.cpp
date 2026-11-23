#include "Tests/InventoryTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
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

		It("moves what blocks a two-handed item to storage", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			const FInventoryItemHandle Rifle = AddTestItem(*Registry, Data->Rifle, Grid);
			Registry->EquipItem(Knife, Hands, Slot_OffHand);

			const EInventoryError Error = Registry->EquipItem(Rifle, Hands, Slot_MainHand, MakeArrayView(&Grid, 1));
			TestTrue(*DescribeErrors(TEXT("Equipping the rifle"), Error, EInventoryError::None), Error == EInventoryError::None);
			TestTrue(TEXT("Rifle in the main hand"), Registry->GetItemAt(Hands, FInventoryPlacement::AtIndex(0)) == Rifle);
			TestTrue(TEXT("Knife back in the grid"), Registry->FindItem(Knife)->Container == Grid);
		});

		It("lets what makes way use the space the item leaves", [this]()
		{
			UInventoryContainerLayout* RifleSized = Data->Make<UInventoryContainerLayout>();
			RifleSized->GridSize = FIntPoint(1, 3);
			const FInventoryContainerHandle Full = Registry->CreateContainer(RifleSized, FGameplayTag(), nullptr);
			const FInventoryItemHandle Rifle = AddTestItem(*Registry, Data->Rifle, Full);
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			Registry->EquipItem(Knife, Hands, Slot_MainHand);

			const EInventoryError Error = Registry->EquipItem(Rifle, Hands, Slot_MainHand, MakeArrayView(&Full, 1));
			TestTrue(*DescribeErrors(TEXT("Swapping into a full grid"), Error, EInventoryError::None), Error == EInventoryError::None);
			TestTrue(TEXT("Knife where the rifle was"), Registry->FindItem(Knife)->Container == Full);
		});
	});

	Describe("Containers", [this]()
	{
		It("keep their items when given a bigger layout", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(3, 2)));

			UInventoryContainerLayout* Bigger = Data->Make<UInventoryContainerLayout>();
			Bigger->GridSize = FIntPoint(6, 6);
			TestTrue(TEXT("Grown"), Registry->SetContainerLayout(Grid, Bigger) == EInventoryError::None);
			TestTrue(TEXT("Knife in place"), Registry->GetItemPlacement(Knife).Cell == FIntPoint(3, 2));
			TestTrue(TEXT("New cells usable"), Registry->CanPlace(Knife, Grid, FInventoryPlacement::AtCell(FIntPoint(5, 4))) == EInventoryError::None);
		});

		It("refuse a layout that would cut an item off", [this]()
		{
			AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(3, 2)));

			const EInventoryError Error = Registry->SetContainerLayout(Grid, Data->Grid2x2);
			TestTrue(*DescribeErrors(TEXT("Shrinking under the knife"), Error, EInventoryError::OutOfBounds), Error == EInventoryError::OutOfBounds);
			TestTrue(TEXT("Old layout kept"), Registry->FindContainer(Grid)->Layout.Get() == Data->Grid4x4);
		});

		It("never put items on locked cells", [this]()
		{
			// 4 x 2 starting with six usable cells: the last two cells of the bottom row are locked.
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr, 6);
			TestEqual(TEXT("Starting capacity"), Registry->GetCapacity(Inventory), 6);
			TestEqual(TEXT("Max capacity"), Registry->GetMaxCapacity(Inventory), 8);
			TestTrue(TEXT("Last usable cell"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(1, 1))));
			TestFalse(TEXT("First locked cell"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(2, 1))));

			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Inventory);
			const EInventoryError OntoLocked = Registry->CanPlace(Stone, Inventory, FInventoryPlacement::AtCell(FIntPoint(3, 1)));
			TestTrue(*DescribeErrors(TEXT("Stone onto a locked cell"), OntoLocked, EInventoryError::OutOfBounds), OntoLocked == EInventoryError::OutOfBounds);

			// Standing up at (2, 0), the knife would reach down into a locked cell.
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			const EInventoryError KnifeOverLocked = Registry->CanPlace(Knife, Inventory, FInventoryPlacement::AtCell(FIntPoint(2, 0)));
			TestTrue(*DescribeErrors(TEXT("Knife over a locked cell"), KnifeOverLocked, EInventoryError::OutOfBounds), KnifeOverLocked == EInventoryError::OutOfBounds);

			for (int32 Index = 1; Index < 6; ++Index)
			{
				TestTrue(*FString::Printf(TEXT("Stone %d added"), Index + 1), AddTestItem(*Registry, Data->Stone, Inventory).IsSet());
			}
			TestFalse(TEXT("No room for a seventh stone"), AddTestItem(*Registry, Data->Stone, Inventory).IsSet());
		});

		It("take a new layout's capacity when given one", [this]()
		{
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr, 6);
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Inventory, 1, FInventoryPlacement::AtCell(FIntPoint(1, 1)));

			TestTrue(TEXT("Given a bigger layout"), Registry->SetContainerLayout(Inventory, Data->Grid4x4) == EInventoryError::None);
			TestEqual(TEXT("The new layout's capacity"), Registry->GetCapacity(Inventory), 16);
			TestTrue(TEXT("Stone in place"), Registry->GetItemPlacement(Stone).Cell == FIntPoint(1, 1));
			TestTrue(TEXT("Moved onto a cell that was locked"), Registry->MoveItem(Stone, Inventory, FInventoryPlacement::AtCell(FIntPoint(3, 1))) == EInventoryError::None);
		});

		It("unlock cells at runtime, row by row, within one layout", [this]()
		{
			// 4 x 2 starting with four usable cells: the whole bottom row is locked.
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr, 4);
			TestEqual(TEXT("Usable cells to start with"), Registry->GetCapacity(Inventory), 4);
			TestFalse(TEXT("Bottom row locked"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(0, 1))));

			TestTrue(TEXT("Two more unlocked"), Registry->AddCapacity(Inventory, 2) == EInventoryError::None);
			TestEqual(TEXT("Usable cells after unlocking"), Registry->GetCapacity(Inventory), 6);
			TestTrue(TEXT("Second cell of the bottom row usable"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(1, 1))));
			TestFalse(TEXT("Third cell of the bottom row still locked"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(2, 1))));
			TestEqual(TEXT("The layout is unchanged"), Data->Grid4x2->GetStartingCapacity(), 8);

			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Inventory, 1, FInventoryPlacement::AtCell(FIntPoint(1, 1)));
			TestTrue(TEXT("Stone on a newly unlocked cell"), Stone.IsSet());

			TestTrue(TEXT("More than the grid unlocks every cell"), Registry->SetCapacity(Inventory, 100) == EInventoryError::None);
			TestEqual(TEXT("Every cell usable"), Registry->GetCapacity(Inventory), 8);
		});

		It("stop growing once every cell is unlocked", [this]()
		{
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr, 7);

			TestTrue(TEXT("Two asked for, the last one unlocked"), Registry->AddCapacity(Inventory, 2) == EInventoryError::None);
			TestEqual(TEXT("Every cell usable"), Registry->GetCapacity(Inventory), 8);

			const EInventoryError Full = Registry->AddCapacity(Inventory, 1);
			TestTrue(*DescribeErrors(TEXT("Growing a full grid"), Full, EInventoryError::AtMaxCapacity), Full == EInventoryError::AtMaxCapacity);

			// A list without a limit has nothing left to unlock, and a list that holds nothing has nothing left to lock.
			UInventoryContainerLayout* OpenList = Data->Make<UInventoryContainerLayout>();
			OpenList->Type = EInventoryContainerType::List;
			const FInventoryContainerHandle Unlimited = Registry->CreateContainer(OpenList, FGameplayTag(), nullptr);
			const EInventoryError GrowUnlimited = Registry->AddCapacity(Unlimited, 1);
			TestTrue(*DescribeErrors(TEXT("Growing a list without a limit"), GrowUnlimited, EInventoryError::AtMaxCapacity), GrowUnlimited == EInventoryError::AtMaxCapacity);

			const FInventoryContainerHandle Closed = Registry->CreateContainer(Data->SmallList, FGameplayTag(), nullptr, 0);
			const EInventoryError ShrinkClosed = Registry->AddCapacity(Closed, -1);
			TestTrue(*DescribeErrors(TEXT("Locking a list that holds nothing"), ShrinkClosed, EInventoryError::InvalidCount), ShrinkClosed == EInventoryError::InvalidCount);
		});

		It("grow with upgrades a whole row at a time, up to Max Rows", [this]()
		{
			// Starts as 4 x 2, and can grow to 4 x 4.
			UInventoryContainerLayout* Growing = Data->Make<UInventoryContainerLayout>();
			Growing->GridSize = FIntPoint(4, 2);
			Growing->GridGrowth = EInventoryGridGrowth::WithUpgrades;
			Growing->MaxRows = 4;
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Growing, FGameplayTag(), nullptr);
			TestEqual(TEXT("Starts with Grid Size's cells"), Registry->GetCapacity(Inventory), 8);
			TestEqual(TEXT("Can grow to Max Rows"), Registry->GetMaxCapacity(Inventory), 16);
			TestEqual(TEXT("Has Grid Size's rows"), Registry->GetGridRows(Inventory), 2);
			TestFalse(TEXT("No third row yet"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(0, 2))));

			// Two more cells reach into a third row, which comes whole, its last two cells locked.
			TestTrue(TEXT("Two more cells"), Registry->AddCapacity(Inventory, 2) == EInventoryError::None);
			TestEqual(TEXT("A third row"), Registry->GetGridRows(Inventory), 3);
			TestTrue(TEXT("Its second cell usable"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(1, 2))));
			TestFalse(TEXT("Its third cell locked"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(2, 2))));

			// New items fill the grid it started with, then the new cells.
			for (int32 Index = 0; Index < 10; ++Index)
			{
				TestTrue(*FString::Printf(TEXT("Stone %d added"), Index + 1), AddTestItem(*Registry, Data->Stone, Inventory).IsSet());
			}
			TestFalse(TEXT("No room for an eleventh stone"), AddTestItem(*Registry, Data->Stone, Inventory).IsSet());
			TestTrue(TEXT("A stone on the new row"), Registry->GetItemAt(Inventory, FInventoryPlacement::AtCell(FIntPoint(1, 2))).IsSet());

			// It stops at Max Rows.
			TestTrue(TEXT("Grown to the end"), Registry->AddCapacity(Inventory, 100) == EInventoryError::None);
			TestEqual(TEXT("Every cell of 4 x 4"), Registry->GetCapacity(Inventory), 16);
			TestEqual(TEXT("Four rows"), Registry->GetGridRows(Inventory), 4);
			const EInventoryError Full = Registry->AddCapacity(Inventory, 1);
			TestTrue(*DescribeErrors(TEXT("Growing past Max Rows"), Full, EInventoryError::AtMaxCapacity), Full == EInventoryError::AtMaxCapacity);

			// Grid Size's rows always count, however few cells are usable, and a container can start grown.
			TestEqual(TEXT("Grid Size's rows with nothing usable"), Growing->GetGridRowsAtCapacity(0), 2);
			TestEqual(TEXT("Started with ten cells"), Registry->GetGridRows(Registry->CreateContainer(Growing, FGameplayTag(), nullptr, 10)), 3);

			// Max Rows below Grid Size's rows adds nothing, and nor does it without growth.
			UInventoryContainerLayout* Fixed = Data->Make<UInventoryContainerLayout>();
			Fixed->GridSize = FIntPoint(4, 2);
			Fixed->GridGrowth = EInventoryGridGrowth::WithUpgrades;
			Fixed->MaxRows = 1;
			TestEqual(TEXT("Max Rows below Grid Size"), Fixed->GetMaxCapacity(), 8);
			Fixed->GridGrowth = EInventoryGridGrowth::None;
			Fixed->MaxRows = 5;
			TestEqual(TEXT("Max Rows without growth"), Fixed->GetMaxCapacity(), 8);
		});

		It("grow with upgrades without a limit when Max Rows is 0", [this]()
		{
			UInventoryContainerLayout* Endless = Data->Make<UInventoryContainerLayout>();
			Endless->GridSize = FIntPoint(4, 2);
			Endless->GridGrowth = EInventoryGridGrowth::WithUpgrades;
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Endless, FGameplayTag(), nullptr);
			TestEqual(TEXT("No most"), Registry->GetMaxCapacity(Inventory), INDEX_NONE);

			TestTrue(TEXT("A thousand more cells"), Registry->AddCapacity(Inventory, 1000) == EInventoryError::None);
			TestEqual(TEXT("All of them usable"), Registry->GetCapacity(Inventory), 1008);
			TestEqual(TEXT("Rows for all of them"), Registry->GetGridRows(Inventory), 252);
			TestTrue(TEXT("The last cell usable"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(3, 251))));
			TestFalse(TEXT("None past it"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(0, 252))));
			TestTrue(TEXT("A stone at the bottom"), AddTestItem(*Registry, Data->Stone, Inventory, 1, FInventoryPlacement::AtCell(FIntPoint(3, 251))).IsSet());
		});

		It("grow to fit their items, and lose empty rows again", [this]()
		{
			UInventoryContainerLayout* Bag = Data->Make<UInventoryContainerLayout>();
			Bag->GridSize = FIntPoint(4, 2);
			Bag->GridGrowth = EInventoryGridGrowth::ToFitItems;
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Bag, FGameplayTag(), nullptr);
			TestEqual(TEXT("No limit"), Registry->GetCapacity(Inventory), INDEX_NONE);
			TestEqual(TEXT("Starts at Grid Size"), Registry->GetGridRows(Inventory), 2);

			// The ninth stone doesn't fit 4 x 2, so the grid adds a row for it.
			TArray<FInventoryItemHandle> Stones;
			for (int32 Index = 0; Index < 9; ++Index)
			{
				const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Inventory);
				TestTrue(*FString::Printf(TEXT("Stone %d added"), Index + 1), Stone.IsSet());
				Stones.Add(Stone);
			}
			TestEqual(TEXT("A third row"), Registry->GetGridRows(Inventory), 3);
			TestTrue(TEXT("The ninth stone on it"), Registry->GetItemPlacement(Stones.Last()).Cell == FIntPoint(0, 2));

			// Put far below, the grid grows down to it; brought back, the empty rows go again.
			TestTrue(TEXT("Moved far down"), Registry->MoveItem(Stones[0], Inventory, FInventoryPlacement::AtCell(FIntPoint(3, 9))) == EInventoryError::None);
			TestEqual(TEXT("Grown to ten rows"), Registry->GetGridRows(Inventory), 10);
			TestTrue(TEXT("Moved back up"), Registry->MoveItem(Stones[0], Inventory, FInventoryPlacement::AtCell(FIntPoint(0, 0))) == EInventoryError::None);
			TestEqual(TEXT("Back to three rows"), Registry->GetGridRows(Inventory), 3);

			// Never fewer rows than Grid Size.
			Registry->RemoveItem(Stones.Last());
			TestEqual(TEXT("Back to Grid Size"), Registry->GetGridRows(Inventory), 2);
		});

		It("stop growing to fit at Max Rows", [this]()
		{
			UInventoryContainerLayout* Bag = Data->Make<UInventoryContainerLayout>();
			Bag->GridSize = FIntPoint(4, 2);
			Bag->GridGrowth = EInventoryGridGrowth::ToFitItems;
			Bag->MaxRows = 3;
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Bag, FGameplayTag(), nullptr);
			TestEqual(TEXT("Holds three rows"), Registry->GetCapacity(Inventory), 12);
			for (int32 Index = 0; Index < 12; ++Index)
			{
				TestTrue(*FString::Printf(TEXT("Stone %d added"), Index + 1), AddTestItem(*Registry, Data->Stone, Inventory).IsSet());
			}
			TestFalse(TEXT("No thirteenth stone"), AddTestItem(*Registry, Data->Stone, Inventory).IsSet());
			TestFalse(TEXT("Nothing past Max Rows"), Registry->IsPlacementUsable(Inventory, FInventoryPlacement::AtCell(FIntPoint(0, 3))));
		});

		It("let items stack higher with a stack bonus", [this]()
		{
			const int32 BaseStack = Data->Ammo->GetMaxStackSize();
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr);
			FInventoryItemQuery AmmoOnly;
			AmmoOnly.Definitions.Add(Data->Ammo);
			TestTrue(TEXT("Bonus added"), Registry->AddStackBonus(Inventory, AmmoOnly, 5) == EInventoryError::None);
			TestTrue(TEXT("A bonus of nothing refused"), Registry->AddStackBonus(Inventory, AmmoOnly, 0) == EInventoryError::InvalidCount);

			// One stack holds five more here.
			TArray<FInventoryItemHandle> Added;
			TestEqual(TEXT("All added"), Registry->AddItemToContainers(FInventoryItemSpec(Data->Ammo, BaseStack + 5), MakeArrayView(&Inventory, 1), false, &Added), BaseStack + 5);
			TestEqual(TEXT("In one stack"), Added.Num(), 1);
			if (Added.Num() != 1)
			{
				return;
			}
			TestEqual(TEXT("Its limit here"), Registry->GetMaxStackSize(Added[0]), BaseStack + 5);
			TestEqual(TEXT("Other items don't stack higher"), Registry->GetMaxStackSize(AddTestItem(*Registry, Data->Herb, Inventory)), Data->Herb->GetMaxStackSize());

			// Moved to a container without the bonus, the stack stays whole, but takes no more there.
			TestTrue(TEXT("Moved whole"), Registry->MoveItem(Added[0], Grid, TOptional<FInventoryPlacement>()) == EInventoryError::None);
			TestEqual(TEXT("Still one stack"), Registry->FindItem(Added[0])->Count, BaseStack + 5);
			TestEqual(TEXT("Its limit there"), Registry->GetMaxStackSize(Added[0]), BaseStack);
		});

		It("weigh items, and what's inside them, for a carry limit", [this]()
		{
			UInventoryItemDefinition* Brick = Data->Make<UInventoryItemDefinition>();
			Brick->TagValues.Emplace(Stat_Weight, 2.0f);

			const FInventoryItemHandle Loose = AddTestItem(*Registry, Brick, Grid);
			TestEqual(TEXT("A brick weighs its value"), Registry->GetCarriedValue(*Registry->FindItem(Loose), Stat_Weight), 2.0f);

			const FInventoryItemHandle Backpack = AddTestItem(*Registry, Data->Backpack, Grid);
			const FInventoryItemRecord* BackpackRecord = Registry->FindItem(Backpack);
			TestTrue(TEXT("Backpack made"), BackpackRecord && BackpackRecord->Containers.Num() == 1);
			if (!BackpackRecord || BackpackRecord->Containers.Num() != 1)
			{
				return;
			}
			AddTestItem(*Registry, Brick, BackpackRecord->Containers[0]);
			TestEqual(TEXT("A backpack weighs what's inside it"), Registry->GetCarriedValue(*Registry->FindItem(Backpack), Stat_Weight), 2.0f);

			// The Inventory Subsystem gives the registry an entry check like this one for each Inventory Component's carry limit.
			const FInventoryContainerHandle Carried = Registry->CreateContainer(Data->Grid4x4, FGameplayTag(), nullptr);
			Registry->CanEnterHook = [Carried](const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container)
			{
				const float Total = InRegistry.GetTotalTagValue(MakeArrayView(&Carried, 1), Stat_Weight, true);
				const bool bTooHeavy = Container == Carried && Total + InRegistry.GetCarriedValue(Item, Stat_Weight) > 5.0f;
				return bTooHeavy ? EInventoryError::OverCarryLimit : EInventoryError::None;
			};
			TestTrue(TEXT("A first brick"), AddTestItem(*Registry, Brick, Carried).IsSet());
			TestTrue(TEXT("A second brick"), AddTestItem(*Registry, Brick, Carried).IsSet());
			TestFalse(TEXT("A third is too heavy"), AddTestItem(*Registry, Brick, Carried).IsSet());
			const EInventoryError MoveError = Registry->MoveItem(Loose, Carried, TOptional<FInventoryPlacement>());
			TestTrue(*DescribeErrors(TEXT("Moving one in is too"), MoveError, EInventoryError::OverCarryLimit), MoveError == EInventoryError::OverCarryLimit);
			Registry->CanEnterHook = nullptr;
		});

		It("go back to the layout's capacity with a negative count", [this]()
		{
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr, 4);

			Registry->SetCapacity(Inventory, 6);
			TestTrue(TEXT("Back to the layout's"), Registry->SetCapacity(Inventory, -1) == EInventoryError::None);
			TestEqual(TEXT("The layout's capacity"), Registry->GetCapacity(Inventory), 8);
		});

		It("refuse to lock a cell an item sits on", [this]()
		{
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr);
			AddTestItem(*Registry, Data->Stone, Inventory, 1, FInventoryPlacement::AtCell(FIntPoint(3, 1)));

			const EInventoryError Error = Registry->SetCapacity(Inventory, 6);
			TestTrue(*DescribeErrors(TEXT("Locking the stone's cell"), Error, EInventoryError::OutOfBounds), Error == EInventoryError::OutOfBounds);
			TestEqual(TEXT("Nothing locked"), Registry->GetCapacity(Inventory), 8);
		});

		It("start with a capacity of their own", [this]()
		{
			UInventoryContainerLayout* Whole = Data->Make<UInventoryContainerLayout>();
			Whole->GridSize = FIntPoint(4, 3);
			const FInventoryContainerHandle Smaller = Registry->CreateContainer(Whole, FGameplayTag(), nullptr, 8);
			const FInventoryContainerHandle Default = Registry->CreateContainer(Whole, FGameplayTag(), nullptr);
			TestEqual(TEXT("Starting capacity given"), Registry->GetCapacity(Smaller), 8);
			TestEqual(TEXT("The layout's otherwise"), Registry->GetCapacity(Default), 12);
			TestEqual(TEXT("More than the grid has is all of it"), Registry->GetCapacity(Registry->CreateContainer(Whole, FGameplayTag(), nullptr, 50)), 12);

			// Items carrying containers can start them smaller than their layout, per item kind.
			UInventoryItemDefinition* Bag = Data->Make<UInventoryItemDefinition>();
			FInventoryItemContainer& BagSpace = Bag->Containers.AddDefaulted_GetRef();
			BagSpace.Layout = Whole;
			BagSpace.bOverrideStartingCapacity = true;
			BagSpace.StartingCapacity = 4;
			const FInventoryItemHandle BagItem = AddTestItem(*Registry, Bag, Grid);
			const FInventoryItemRecord* BagRecord = Registry->FindItem(BagItem);
			TestTrue(TEXT("Bag made"), BagRecord && BagRecord->Containers.Num() == 1);
			if (BagRecord && BagRecord->Containers.Num() == 1)
			{
				TestEqual(TEXT("Bag starts with one row"), Registry->GetCapacity(BagRecord->Containers[0]), 4);
			}
		});

		It("lock slots past Usable Slots, and unlock them at runtime", [this]()
		{
			// Two plain slots, only the first usable to start with.
			UInventoryContainerLayout* Belt = Data->Make<UInventoryContainerLayout>();
			Belt->Type = EInventoryContainerType::Slots;
			for (const FGameplayTag& SlotTag : { FGameplayTag(Slot_MainHand), FGameplayTag(Slot_OffHand) })
			{
				Belt->Slots.AddDefaulted_GetRef().Tag = SlotTag;
			}
			Belt->UsableSlots = 1;
			const FInventoryContainerHandle Slots = Registry->CreateContainer(Belt, FGameplayTag(), nullptr);
			TestEqual(TEXT("Usable slots from the layout"), Registry->GetCapacity(Slots), 1);
			TestFalse(TEXT("Second slot locked"), Registry->IsPlacementUsable(Slots, FInventoryPlacement::AtIndex(1)));

			TestTrue(TEXT("First stone in the first slot"), AddTestItem(*Registry, Data->Stone, Slots).IsSet());
			TestFalse(TEXT("No room for a second stone"), AddTestItem(*Registry, Data->Stone, Slots).IsSet());
			const FInventoryItemHandle Loose = AddTestItem(*Registry, Data->Stone, Grid);
			const EInventoryError IntoLocked = Registry->CanPlace(Loose, Slots, FInventoryPlacement::AtIndex(1));
			TestTrue(*DescribeErrors(TEXT("Stone into a locked slot"), IntoLocked, EInventoryError::OutOfBounds), IntoLocked == EInventoryError::OutOfBounds);

			TestTrue(TEXT("Second slot unlocked"), Registry->SetCapacity(Slots, 2) == EInventoryError::None);
			TestTrue(TEXT("Second stone in the second slot"), Registry->MoveItem(Loose, Slots, FInventoryPlacement::AtIndex(1)) == EInventoryError::None);

			const EInventoryError Relock = Registry->SetCapacity(Slots, 1);
			TestTrue(*DescribeErrors(TEXT("Locking the second stone's slot"), Relock, EInventoryError::OutOfBounds), Relock == EInventoryError::OutOfBounds);
			TestEqual(TEXT("Still two usable slots"), Registry->GetCapacity(Slots), 2);
		});

		It("grow and shrink a list's capacity at runtime", [this]()
		{
			const FInventoryContainerHandle List = Registry->CreateContainer(Data->SmallList, FGameplayTag(), nullptr);
			TestEqual(TEXT("The layout's capacity"), Registry->GetCapacity(List), 2);
			AddTestItem(*Registry, Data->Stone, List);
			AddTestItem(*Registry, Data->Stone, List);
			TestFalse(TEXT("No room for a third stone"), AddTestItem(*Registry, Data->Stone, List).IsSet());

			TestTrue(TEXT("Grown"), Registry->SetCapacity(List, 3) == EInventoryError::None);
			const FInventoryItemHandle Third = AddTestItem(*Registry, Data->Stone, List);
			TestTrue(TEXT("Third stone added"), Third.IsSet());

			const EInventoryError Shrink = Registry->SetCapacity(List, 2);
			TestTrue(*DescribeErrors(TEXT("Shrinking under three stones"), Shrink, EInventoryError::NoSpace), Shrink == EInventoryError::NoSpace);
			TestEqual(TEXT("Still three"), Registry->GetCapacity(List), 3);

			Registry->RemoveItem(Third);
			TestTrue(TEXT("Back to the layout's"), Registry->SetCapacity(List, -1) == EInventoryError::None);
			TestEqual(TEXT("The layout's capacity again"), Registry->GetCapacity(List), 2);
		});

		It("report a list without a limit", [this]()
		{
			UInventoryContainerLayout* Open = Data->Make<UInventoryContainerLayout>();
			Open->Type = EInventoryContainerType::List;
			const FInventoryContainerHandle List = Registry->CreateContainer(Open, FGameplayTag(), nullptr);
			TestEqual(TEXT("No limit"), Registry->GetCapacity(List), static_cast<int32>(INDEX_NONE));
			TestTrue(TEXT("Any position usable"), Registry->IsPlacementUsable(List, FInventoryPlacement::AtIndex(50)));
		});

		It("sort biggest first and merge partial stacks", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(3, 2)));
			AddTestItem(*Registry, Data->Ammo, Grid, 10, FInventoryPlacement::AtCell(FIntPoint(0, 3)));
			AddTestItem(*Registry, Data->Ammo, Grid, 5, FInventoryPlacement::AtCell(FIntPoint(2, 3)));

			TestTrue(TEXT("Sorted"), Registry->SortContainer(Grid) == EInventoryError::None);
			TestTrue(TEXT("Knife in the corner"), Registry->GetItemPlacement(Knife).Cell == FIntPoint(0, 0));

			FInventoryItemQuery AmmoQuery;
			AmmoQuery.Definitions.Add(Data->Ammo);
			TArray<FInventoryItemHandle> AmmoStacks;
			Registry->FindItems(MakeArrayView(&Grid, 1), AmmoQuery, false, AmmoStacks);
			TestEqual(TEXT("One ammo stack"), AmmoStacks.Num(), 1);
			TestEqual(TEXT("All the ammo"), Count(Data->Ammo), 15);
		});

		It("sort lists by name and merge partial stacks", [this]()
		{
			Data->Stone->DisplayName = FText::FromString(TEXT("Stone"));
			Data->Ammo->DisplayName = FText::FromString(TEXT("Ammo"));
			UInventoryContainerLayout* ListLayout = Data->Make<UInventoryContainerLayout>();
			ListLayout->Type = EInventoryContainerType::List;
			const FInventoryContainerHandle List = Registry->CreateContainer(ListLayout, FGameplayTag(), nullptr);

			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, List);
			AddTestItem(*Registry, Data->Ammo, List, 10, FInventoryPlacement::AtIndex(1));
			AddTestItem(*Registry, Data->Ammo, List, 5, FInventoryPlacement::AtIndex(2));

			TestTrue(TEXT("Sorted"), Registry->SortContainer(List) == EInventoryError::None);
			const TArray<FInventoryItemHandle>& Sorted = Registry->FindContainer(List)->Items;
			TestEqual(TEXT("The ammo merged"), Sorted.Num(), 2);
			TestTrue(TEXT("Ammo first"), Sorted.Num() == 2 && Registry->FindItem(Sorted[0])->Definition.Get() == Data->Ammo);
			TestTrue(TEXT("Stone last"), Sorted.Num() == 2 && Sorted[1] == Stone);
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

	Describe("Attachments", [this]()
	{
		It("add their modifiers and tags to the item they're attached to", [this]()
		{
			UInventoryContainerLayout* Mounts = Data->Make<UInventoryContainerLayout>();
			Mounts->Type = EInventoryContainerType::Slots;
			Mounts->bEquipment = true;
			Mounts->Slots.AddDefaulted_GetRef().Tag = Slot_MainHand;

			UInventoryItemDefinition* Gun = Data->Make<UInventoryItemDefinition>();
			Gun->TagValues.Emplace(Stat_Fire, 10.0f);
			FInventoryItemContainer& GunMounts = Gun->Containers.AddDefaulted_GetRef();
			GunMounts.Tag = Container_Main;
			GunMounts.Layout = Mounts;

			UInventoryItemDefinition* Scope = Data->Make<UInventoryItemDefinition>();
			Scope->EquipRules = Data->Make<UInventoryEquipRules>();
			Scope->EquipRules->Slots.AddTag(Slot_MainHand);
			Scope->AttachmentValueModifiers.Emplace(Stat_Fire, 3.0f);
			Scope->AttachmentGrantedTags.AddTag(Type_Herb);

			const FInventoryItemHandle GunItem = AddTestItem(*Registry, Gun, Grid);
			const FInventoryItemHandle ScopeItem = AddTestItem(*Registry, Scope, Registry->FindItemContainer(GunItem, Container_Main), 1, FInventoryPlacement::AtIndex(0));

			TestTrue(TEXT("Scope attached"), ScopeItem.IsSet());
			TestEqual(TEXT("Plain value untouched"), Registry->GetTagValue(GunItem, Stat_Fire), 10.0f);
			TestEqual(TEXT("Effective value with the scope"), Registry->GetEffectiveTagValue(GunItem, Stat_Fire), 13.0f);
			TestTrue(TEXT("Tag from the scope"), Registry->GetEffectiveTags(GunItem).HasTag(Type_Herb));
			TestEqual(TEXT("One attachment"), Registry->GetAttachments(GunItem).Num(), 1);
		});
	});

	Describe("Tag value maximums", [this]()
	{
		It("come from the max tag, or else from the starting value", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			Registry->SetTagValue(Knife, Stat_Durability, 40.0f);
			TestEqual(TEXT("Starting durability"), Registry->GetTagValueMax(Knife, Stat_Durability, Stat_Fire), 100.0f);

			Registry->SetTagValue(Knife, Stat_Fire, 50.0f);
			TestEqual(TEXT("The max tag's value"), Registry->GetTagValueMax(Knife, Stat_Durability, Stat_Fire), 50.0f);
			TestEqual(TEXT("No starting value"), Registry->GetTagValueMax(Knife, Type_Herb, FGameplayTag()), 0.0f);
		});
	});

	Describe("Totals", [this]()
	{
		It("add up a tag value over stacks and nested items", [this]()
		{
			Data->Ammo->TagValues.Emplace(Stat_Durability, 2.0f);
			AddTestItem(*Registry, Data->Ammo, Grid, 10);
			const FInventoryItemHandle Backpack = AddTestItem(*Registry, Data->Backpack, Grid);
			AddTestItem(*Registry, Data->Knife, Registry->FindItemContainer(Backpack, Container_Main));

			TestEqual(TEXT("Each round, and the knife in the backpack"), Registry->GetTotalTagValue(MakeArrayView(&Grid, 1), Stat_Durability, true), 120.0f);
			TestEqual(TEXT("Only the top level"), Registry->GetTotalTagValue(MakeArrayView(&Grid, 1), Stat_Durability, false), 20.0f);
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

		It("uses only the chosen items", [this]()
		{
			const FInventoryItemHandle First = AddTestItem(*Registry, Data->Herb, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));
			const FInventoryItemHandle Second = AddTestItem(*Registry, Data->Herb, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 0)));
			const FInventoryItemHandle Third = AddTestItem(*Registry, Data->Herb, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(2, 0)));
			const TArray<FInventoryItemHandle> Chosen = { Second, Third };
			FRandomStream Stream(1);

			const EInventoryError Error = Registry->CraftFromItems(*MakeMedkitRecipe(2), Chosen, MakeArrayView(&Grid, 1), FGameplayTagContainer(), 1, Stream);
			TestTrue(*DescribeErrors(TEXT("Crafting from two chosen herbs"), Error, EInventoryError::None), Error == EInventoryError::None);
			TestTrue(TEXT("The herb not chosen is kept"), Registry->IsValid(First));
			TestFalse(TEXT("The chosen herbs are used up"), Registry->IsValid(Second) || Registry->IsValid(Third));
		});

		It("is made from items only when every one of them is used", [this]()
		{
			const FInventoryItemHandle First = AddTestItem(*Registry, Data->Herb, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));
			const FInventoryItemHandle Second = AddTestItem(*Registry, Data->Herb, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 0)));
			const TArray<FInventoryItemHandle> Both = { First, Second };
			const TArray<FInventoryItemHandle> One = { First };

			TestTrue(TEXT("Two herbs for two"), Registry->IsMadeFrom(*MakeMedkitRecipe(2), Both, FGameplayTagContainer()));
			TestFalse(TEXT("One herb for two"), Registry->IsMadeFrom(*MakeMedkitRecipe(2), One, FGameplayTagContainer()));
			TestFalse(TEXT("Two herbs for one, one left over"), Registry->IsMadeFrom(*MakeMedkitRecipe(1), Both, FGameplayTagContainer()));
		});
	});

	Describe("Rules and growth", [this]()
	{
		It("check merging into another container like any other way in", [this]()
		{
			const FInventoryContainerHandle Other = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr);
			const FInventoryItemHandle Source = AddTestItem(*Registry, Data->Ammo, Grid, 10);
			const FInventoryItemHandle Target = AddTestItem(*Registry, Data->Ammo, Other, 5);

			// Such as a carry limit on whoever holds the other container.
			Registry->CanEnterHook = [Other](const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container)
			{
				return Container == Other ? EInventoryError::OverCarryLimit : EInventoryError::None;
			};
			const EInventoryError Refused = Registry->MergeStacks(Source, Target, 0);
			TestTrue(*DescribeErrors(TEXT("Merging past the check"), Refused, EInventoryError::OverCarryLimit), Refused == EInventoryError::OverCarryLimit);
			TestEqual(TEXT("Nothing moved"), Registry->FindItem(Target)->Count, 5);

			Registry->CanEnterHook = nullptr;
			TestTrue(TEXT("Merged without it"), Registry->MergeStacks(Source, Target, 0) == EInventoryError::None);
			TestEqual(TEXT("Topped up"), Registry->FindItem(Target)->Count, 15);
		});

		It("say why an item was turned away when no container would take it", [this]()
		{
			Registry->CanEnterHook = [](const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container)
			{
				return EInventoryError::OverCarryLimit;
			};
			EInventoryError Error = EInventoryError::None;
			Registry->AddItemToContainers(FInventoryItemSpec(Data->Stone), MakeArrayView(&Grid, 1), false, nullptr, &Error);
			TestTrue(*DescribeErrors(TEXT("Adding past the check"), Error, EInventoryError::OverCarryLimit), Error == EInventoryError::OverCarryLimit);
			Registry->CanEnterHook = nullptr;

			// A container that would take it but has no room still says No Space.
			const FInventoryContainerHandle Tiny = Registry->CreateContainer(Data->Grid1x1, FGameplayTag(), nullptr);
			AddTestItem(*Registry, Data->Stone, Tiny);
			Registry->AddItemToContainers(FInventoryItemSpec(Data->Stone), MakeArrayView(&Tiny, 1), false, nullptr, &Error);
			TestTrue(*DescribeErrors(TEXT("Adding to a full grid"), Error, EInventoryError::NoSpace), Error == EInventoryError::NoSpace);
		});

		It("refuse placements once their layout's columns change, until given it again", [this]()
		{
			UInventoryContainerLayout* Changing = Data->Make<UInventoryContainerLayout>();
			Changing->GridSize = FIntPoint(4, 2);
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Changing, FGameplayTag(), nullptr);
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Inventory);

			// Such as the asset edited while playing.
			Changing->GridSize = FIntPoint(5, 2);
			const EInventoryError Stale = Registry->CanPlace(Stone, Inventory, FInventoryPlacement::AtCell(FIntPoint(4, 0)));
			TestTrue(*DescribeErrors(TEXT("Placing with the old cells"), Stale, EInventoryError::OutOfBounds), Stale == EInventoryError::OutOfBounds);

			TestTrue(TEXT("Given its layout again"), Registry->SetContainerLayout(Inventory, Changing) == EInventoryError::None);
			TestTrue(TEXT("The new column usable"), Registry->CanPlace(Stone, Inventory, FInventoryPlacement::AtCell(FIntPoint(4, 0))) == EInventoryError::None);
		});

		It("undo a grid's growth with the change that grew it", [this]()
		{
			UInventoryContainerLayout* Bag = Data->Make<UInventoryContainerLayout>();
			Bag->GridSize = FIntPoint(4, 2);
			Bag->GridGrowth = EInventoryGridGrowth::ToFitItems;
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Bag, FGameplayTag(), nullptr);
			for (int32 Index = 0; Index < 8; ++Index)
			{
				AddTestItem(*Registry, Data->Stone, Inventory);
			}

			const EInventoryError Preview = Registry->DryRun([&]()
			{
				EInventoryError AddError = EInventoryError::None;
				Registry->AddItemToContainers(FInventoryItemSpec(Data->Stone), MakeArrayView(&Inventory, 1), false, nullptr, &AddError);
				return AddError;
			});
			TestTrue(TEXT("A ninth stone would fit"), Preview == EInventoryError::None);
			TestEqual(TEXT("Still two rows"), Registry->GetGridRows(Inventory), 2);
			TestEqual(TEXT("And two rows of cells"), Registry->FindContainer(Inventory)->Cells.Num(), 8);
		});

		It("pack a grid that grows to fit when it's sorted", [this]()
		{
			UInventoryContainerLayout* Bag = Data->Make<UInventoryContainerLayout>();
			Bag->GridSize = FIntPoint(4, 2);
			Bag->GridGrowth = EInventoryGridGrowth::ToFitItems;
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Bag, FGameplayTag(), nullptr);
			TArray<FInventoryItemHandle> Stones;
			for (int32 Index = 0; Index < 9; ++Index)
			{
				Stones.Add(AddTestItem(*Registry, Data->Stone, Inventory));
			}

			// The first row emptied leaves the ninth stone alone on the third.
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Registry->RemoveItem(Stones[Index]);
			}
			TestEqual(TEXT("Three rows before"), Registry->GetGridRows(Inventory), 3);
			TestTrue(TEXT("Sorted"), Registry->SortContainer(Inventory) == EInventoryError::None);
			TestEqual(TEXT("Grid Size's two after"), Registry->GetGridRows(Inventory), 2);
		});

		It("take a stack bigger than its definition allows where a bonus lets it", [this]()
		{
			const int32 BaseStack = Data->Ammo->GetMaxStackSize();
			const FInventoryContainerHandle Inventory = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr);
			TestFalse(TEXT("Too big without a bonus"), AddTestItem(*Registry, Data->Ammo, Inventory, BaseStack + 1, FInventoryPlacement::AtCell(FIntPoint(0, 0))).IsSet());

			FInventoryItemQuery Anything;
			TestTrue(TEXT("A bonus for every item"), Registry->AddStackBonus(Inventory, Anything, 5) == EInventoryError::None);
			TestTrue(TEXT("Five more fit"), AddTestItem(*Registry, Data->Ammo, Inventory, BaseStack + 5, FInventoryPlacement::AtCell(FIntPoint(0, 0))).IsSet());
			TestFalse(TEXT("Six more don't"), AddTestItem(*Registry, Data->Ammo, Inventory, BaseStack + 6, FInventoryPlacement::AtCell(FIntPoint(1, 0))).IsSet());
			TestEqual(TEXT("Items that don't stack never do"), Registry->GetMaxStackSize(AddTestItem(*Registry, Data->Stone, Inventory)), 1);
		});

		It("weigh a stack by its count", [this]()
		{
			UInventoryItemDefinition* Brick = Data->Make<UInventoryItemDefinition>();
			Brick->TagValues.Emplace(Stat_Weight, 2.0f);
			Brick->StackRules = Data->Ammo->StackRules;
			const FInventoryItemHandle Bricks = AddTestItem(*Registry, Brick, Grid, 3);
			TestEqual(TEXT("Three bricks"), Registry->GetCarriedValue(*Registry->FindItem(Bricks), Stat_Weight), 6.0f);
			TestEqual(TEXT("In the total"), Registry->GetTotalTagValue(MakeArrayView(&Grid, 1), Stat_Weight, true), 6.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
