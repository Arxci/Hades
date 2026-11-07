#include "Tests/InventoryTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Data/InventoryItemDefinition.h"

using namespace InventoryTests;

BEGIN_DEFINE_SPEC(FInventoryRegistrySpec, "ModularInventory.Registry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FTestData> Data;
	TUniquePtr<FInventoryRegistry> Registry;
	FInventoryContainerHandle Grid;
	FInventoryContainerHandle SmallGrid;

	int32 Count(UInventoryItemDefinition* Definition, FInventoryContainerHandle Container) const
	{
		FInventoryItemQuery Query;
		Query.Definitions.Add(Definition);
		return Registry->CountItems(MakeArrayView(&Container, 1), Query, true);
	}
END_DEFINE_SPEC(FInventoryRegistrySpec)

void FInventoryRegistrySpec::Define()
{
	BeforeEach([this]()
	{
		Data = MakeUnique<FTestData>();
		Registry = MakeUnique<FInventoryRegistry>();
		Grid = Registry->CreateContainer(Data->Grid4x4, FGameplayTag(), nullptr);
		SmallGrid = Registry->CreateContainer(Data->Grid2x2, FGameplayTag(), nullptr);
	});

	AfterEach([this]()
	{
		Registry.Reset();
		Data.Reset();
	});

	Describe("Handles", [this]()
	{
		It("go stale when their item is destroyed, even after its slot is reused", [this]()
		{
			const FInventoryItemHandle First = AddTestItem(*Registry, Data->Stone, Grid);
			Registry->RemoveItem(First);
			const FInventoryItemHandle Second = AddTestItem(*Registry, Data->Stone, Grid);

			TestFalse(TEXT("The destroyed item's handle is valid"), Registry->IsValid(First));
			TestTrue(TEXT("The new item's handle is valid"), Registry->IsValid(Second));
			TestEqual(TEXT("The record slot was reused"), Second.Index, First.Index);
			TestTrue(TEXT("The generation changed"), Second.Generation != First.Generation);
		});
	});

	Describe("Grid", [this]()
	{
		It("places items where they fit and refuses overlaps", [this]()
		{
			TestTrue(TEXT("Rifle placed"), AddTestItem(*Registry, Data->Rifle, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0))).IsSet());

			const EInventoryError Overlap = Registry->AddItem(FInventoryItemSpec(Data->Knife), Grid, FInventoryPlacement::AtCell(FIntPoint(0, 1)));
			TestTrue(*DescribeErrors(TEXT("Knife over the rifle"), Overlap, EInventoryError::Occupied), Overlap == EInventoryError::Occupied);

			TestTrue(TEXT("Knife beside the rifle"), AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(1, 0))).IsSet());

			const EInventoryError Outside = Registry->AddItem(FInventoryItemSpec(Data->Rifle), Grid, FInventoryPlacement::AtCell(FIntPoint(3, 2)));
			TestTrue(*DescribeErrors(TEXT("Rifle hanging off the grid"), Outside, EInventoryError::OutOfBounds), Outside == EInventoryError::OutOfBounds);
		});

		It("rotates in place only when the rotated shape fits", [this]()
		{
			const FInventoryItemHandle Rifle = AddTestItem(*Registry, Data->Rifle, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));
			TestTrue(TEXT("Rotated onto free cells"), Registry->RotateItem(Rifle) == EInventoryError::None);
			TestTrue(TEXT("Rotated size is 3x1"), Registry->GetItemSize(Rifle) == FIntPoint(3, 1));

			AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 1)));
			const EInventoryError Blocked = Registry->RotateItem(Rifle);
			TestTrue(*DescribeErrors(TEXT("Rotating back into the knife"), Blocked, EInventoryError::Occupied), Blocked == EInventoryError::Occupied);
			TestTrue(TEXT("Still rotated"), Registry->GetItemPlacement(Rifle).bRotated);
		});

		It("finds free spots until the grid is full", [this]()
		{
			for (int32 Index = 0; Index < 16; ++Index)
			{
				AddTestItem(*Registry, Data->Stone, Grid);
			}
			TestEqual(TEXT("Stones placed"), Count(Data->Stone, Grid), 16);

			const EInventoryError Full = Registry->AddItem(FInventoryItemSpec(Data->Stone), Grid);
			TestTrue(*DescribeErrors(TEXT("A seventeenth stone"), Full, EInventoryError::NoSpace), Full == EInventoryError::NoSpace);
		});

		It("turns new items sideways when only that fits", [this]()
		{
			const FInventoryContainerHandle Wide = Registry->CreateContainer(Data->Grid4x2, FGameplayTag(), nullptr);
			const FInventoryItemHandle Rifle = AddTestItem(*Registry, Data->Rifle, Wide);

			TestTrue(TEXT("Rifle placed"), Rifle.IsSet());
			TestTrue(TEXT("Rifle rotated"), Registry->GetItemPlacement(Rifle).bRotated);
		});
	});

	Describe("Stacks", [this]()
	{
		It("top up existing stacks before taking new space", [this]()
		{
			AddTestItem(*Registry, Data->Ammo, Grid, 20);
			AddTestItem(*Registry, Data->Ammo, Grid, 15);

			TArray<FInventoryItemHandle> Stacks;
			FInventoryItemQuery Query;
			Query.Definitions.Add(Data->Ammo);
			Registry->FindItems(MakeArrayView(&Grid, 1), Query, false, Stacks);

			TestEqual(TEXT("Total ammo"), Count(Data->Ammo, Grid), 35);
			TestEqual(TEXT("Stacks"), Stacks.Num(), 2);
		});

		It("split and merge", [this]()
		{
			const FInventoryItemHandle Stack = AddTestItem(*Registry, Data->Ammo, Grid, 20);
			FInventoryItemHandle Split;
			TestTrue(TEXT("Split"), Registry->SplitStack(Stack, 5, FInventoryContainerHandle(), {}, &Split) == EInventoryError::None);
			TestEqual(TEXT("Left behind"), Registry->FindItem(Stack)->Count, 15);
			TestEqual(TEXT("Split off"), Registry->FindItem(Split)->Count, 5);

			int32 Moved = 0;
			TestTrue(TEXT("Merged"), Registry->MergeStacks(Split, Stack, 0, &Moved) == EInventoryError::None);
			TestEqual(TEXT("Moved"), Moved, 5);
			TestEqual(TEXT("Merged count"), Registry->FindItem(Stack)->Count, 20);
			TestFalse(TEXT("Emptied stack destroyed"), Registry->IsValid(Split));
		});

		It("stay apart when their state differs", [this]()
		{
			FInventoryItemSpec Worn(Data->Ammo, 10);
			Worn.TagValues.Emplace(Stat_Durability, 1.0f);
			TArray<FInventoryItemHandle> Added;
			Registry->AddItem(Worn, Grid, {}, &Added);
			const FInventoryItemHandle Fresh = AddTestItem(*Registry, Data->Ammo, Grid, 10);

			TestTrue(TEXT("Two stacks"), Added.Num() == 1 && Fresh.IsSet() && Added[0] != Fresh);
			const EInventoryError Merge = Registry->MergeStacks(Fresh, Added[0]);
			TestTrue(*DescribeErrors(TEXT("Merging different state"), Merge, EInventoryError::CannotStack), Merge == EInventoryError::CannotStack);
		});

		It("add all or nothing unless partial adds are allowed", [this]()
		{
			const FInventoryContainerHandle Tiny = Registry->CreateContainer(Data->Grid1x1, FGameplayTag(), nullptr);

			const EInventoryError Error = Registry->AddItem(FInventoryItemSpec(Data->Ammo, 60), Tiny);
			TestTrue(*DescribeErrors(TEXT("Two stacks into one cell"), Error, EInventoryError::NoSpace), Error == EInventoryError::NoSpace);
			TestEqual(TEXT("Nothing left behind"), Count(Data->Ammo, Tiny), 0);

			const int32 Added = Registry->AddItemToContainers(FInventoryItemSpec(Data->Ammo, 60), MakeArrayView(&Tiny, 1), true);
			TestEqual(TEXT("Partial add"), Added, 30);
		});
	});

	Describe("Nesting", [this]()
	{
		It("creates an item's containers and destroys them with it", [this]()
		{
			const FInventoryItemHandle Backpack = AddTestItem(*Registry, Data->Backpack, Grid);
			const FInventoryContainerHandle Storage = Registry->FindItemContainer(Backpack, Container_Main);
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Storage);

			TestTrue(TEXT("Backpack storage exists"), Registry->IsValid(Storage));
			TestTrue(TEXT("Stone stored in the backpack"), Stone.IsSet());

			Registry->RemoveItem(Backpack);
			TestFalse(TEXT("Storage destroyed"), Registry->IsValid(Storage));
			TestFalse(TEXT("Stone destroyed"), Registry->IsValid(Stone));
		});

		It("refuses to put an item inside itself", [this]()
		{
			const FInventoryItemHandle Outer = AddTestItem(*Registry, Data->Backpack, Grid);
			const FInventoryContainerHandle OuterStorage = Registry->FindItemContainer(Outer, Container_Main);
			const FInventoryItemHandle Inner = AddTestItem(*Registry, Data->Backpack, OuterStorage);
			const FInventoryContainerHandle InnerStorage = Registry->FindItemContainer(Inner, Container_Main);

			const EInventoryError IntoInner = Registry->MoveItem(Outer, InnerStorage);
			TestTrue(*DescribeErrors(TEXT("Into a backpack it holds"), IntoInner, EInventoryError::WouldContainItself), IntoInner == EInventoryError::WouldContainItself);

			const EInventoryError IntoSelf = Registry->MoveItem(Outer, OuterStorage);
			TestTrue(*DescribeErrors(TEXT("Into its own storage"), IntoSelf, EInventoryError::WouldContainItself), IntoSelf == EInventoryError::WouldContainItself);
		});

		It("moves whole subtrees with their handles intact", [this]()
		{
			const FInventoryItemHandle Backpack = AddTestItem(*Registry, Data->Backpack, Grid);
			const FInventoryContainerHandle Storage = Registry->FindItemContainer(Backpack, Container_Main);
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Storage);

			TestTrue(TEXT("Moved"), Registry->MoveItem(Backpack, SmallGrid) == EInventoryError::None);
			TestTrue(TEXT("Stone still valid"), Registry->IsValid(Stone));
			TestTrue(TEXT("Stone now under the other root"), Registry->GetRootContainer(Registry->FindItem(Stone)->Container) == SmallGrid);
		});
	});

	Describe("Swap", [this]()
	{
		It("exchanges items of different sizes across containers", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, SmallGrid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));

			TestTrue(TEXT("Swapped"), Registry->SwapItems(Knife, Stone) == EInventoryError::None);
			TestTrue(TEXT("Knife moved"), Registry->FindItem(Knife)->Container == SmallGrid);
			TestTrue(TEXT("Stone moved"), Registry->FindItem(Stone)->Container == Grid);
		});

		It("changes nothing when one side doesn't fit", [this]()
		{
			const FInventoryItemHandle Rifle = AddTestItem(*Registry, Data->Rifle, Grid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, SmallGrid, 1, FInventoryPlacement::AtCell(FIntPoint(0, 0)));

			const EInventoryError Error = Registry->SwapItems(Rifle, Stone);
			TestTrue(*DescribeErrors(TEXT("Rifle into a 2x2 grid"), Error, EInventoryError::OutOfBounds), Error == EInventoryError::OutOfBounds);
			TestTrue(TEXT("Rifle stayed"), Registry->FindItem(Rifle)->Container == Grid);
			TestTrue(TEXT("Stone stayed"), Registry->FindItem(Stone)->Container == SmallGrid);
			TestTrue(TEXT("Rifle cells restored"), Registry->GetItemAt(Grid, FInventoryPlacement::AtCell(FIntPoint(0, 2))) == Rifle);
		});
	});

	Describe("Lists", [this]()
	{
		It("respect capacity and keep their order", [this]()
		{
			const FInventoryContainerHandle List = Registry->CreateContainer(Data->SmallList, FGameplayTag(), nullptr);
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, List);
			const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, List);

			const EInventoryError Full = Registry->AddItem(FInventoryItemSpec(Data->Medkit), List);
			TestTrue(*DescribeErrors(TEXT("A third item"), Full, EInventoryError::NoSpace), Full == EInventoryError::NoSpace);

			TestTrue(TEXT("Moved to the front"), Registry->MoveItem(Stone, List, FInventoryPlacement::AtIndex(0)) == EInventoryError::None);
			TestEqual(TEXT("Stone first"), Registry->GetItemPlacement(Stone).Index, 0);
			TestEqual(TEXT("Knife second"), Registry->GetItemPlacement(Knife).Index, 1);
		});
	});

	Describe("Tag values", [this]()
	{
		It("read the definition's value and only store changes", [this]()
		{
			const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Grid);
			TestEqual(TEXT("Default"), Registry->GetTagValue(Knife, Stat_Durability), 100.0f);

			Registry->SetTagValue(Knife, Stat_Durability, 40.0f);
			TestEqual(TEXT("Changed"), Registry->GetTagValue(Knife, Stat_Durability), 40.0f);
			TestEqual(TEXT("Stored once changed"), Registry->FindItem(Knife)->TagValues.Num(), 1);

			Registry->SetTagValue(Knife, Stat_Durability, 100.0f);
			TestEqual(TEXT("Not stored when back to the default"), Registry->FindItem(Knife)->TagValues.Num(), 0);

			Registry->AddToTagValue(Knife, Stat_Durability, -30.0f);
			TestEqual(TEXT("Added"), Registry->GetTagValue(Knife, Stat_Durability), 70.0f);

			Registry->RemoveTagValue(Knife, Stat_Durability);
			TestEqual(TEXT("Removed"), Registry->GetTagValue(Knife, Stat_Durability), 100.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
