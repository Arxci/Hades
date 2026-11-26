#include "Tests/InventoryTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Data/InventoryItemDefinition.h"
#include "Shortcuts/InventoryShortcutComponent.h"

using namespace InventoryTests;

BEGIN_DEFINE_SPEC(FInventoryShortcutSpec, "ModularInventory.Shortcuts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FTestData> Data;
	TUniquePtr<FInventoryRegistry> Registry;
	/** The owner's only root container. */
	FInventoryContainerHandle Pockets;
	/** Someone else's, such as a chest's. */
	FInventoryContainerHandle Elsewhere;
	FInventoryShortcutRules Rules;
	FInventoryShortcutSlots Slots;

	TArray<FInventoryContainerHandle> OwnerRoots() const
	{
		return { Pockets };
	}

	bool RefreshSlots()
	{
		return Slots.Refresh(*Registry, OwnerRoots(), Rules);
	}
END_DEFINE_SPEC(FInventoryShortcutSpec)

void FInventoryShortcutSpec::Define()
{
	BeforeEach([this]()
	{
		Data = MakeUnique<FTestData>();
		Registry = MakeUnique<FInventoryRegistry>();
		Pockets = Registry->CreateContainer(Data->Grid4x4, FGameplayTag(), nullptr);
		Elsewhere = Registry->CreateContainer(Data->Grid2x2, FGameplayTag(), nullptr);
		Rules = FInventoryShortcutRules();
		Rules.Slots.SetNum(3);
		Slots = FInventoryShortcutSlots();
		Slots.SetNum(3);
	});

	AfterEach([this]()
	{
		Registry.Reset();
		Data.Reset();
	});

	It("put an item on one slot at a time", [this]()
	{
		const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Pockets);
		TestTrue(TEXT("On the first slot"), Slots.Assign(*Registry, OwnerRoots(), Rules, 0, Knife));
		TestTrue(TEXT("Moved to the second slot"), Slots.Assign(*Registry, OwnerRoots(), Rules, 1, Knife));
		TestFalse(TEXT("Off the first slot"), Slots.Get(0).IsSet());
		TestEqual(TEXT("Found on the second slot"), Slots.Find(Knife), 1);
	});

	It("refuse items the owner doesn't hold, or a slot doesn't take", [this]()
	{
		const FInventoryItemHandle Theirs = AddTestItem(*Registry, Data->Stone, Elsewhere);
		TestFalse(TEXT("Someone else's stone"), Slots.Assign(*Registry, OwnerRoots(), Rules, 0, Theirs));

		Rules.Slots[0].Filter.Definitions.Add(Data->Knife);
		const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Pockets);
		TestFalse(TEXT("A stone on the knife slot"), Slots.Assign(*Registry, OwnerRoots(), Rules, 0, Stone));
		TestTrue(TEXT("A stone on another slot"), Slots.Assign(*Registry, OwnerRoots(), Rules, 1, Stone));
		TestEqual(TEXT("A free slot that takes it"), Slots.AssignToFreeSlot(*Registry, OwnerRoots(), Rules, AddTestItem(*Registry, Data->Stone, Pockets)), 2);
	});

	It("ask its slot filter, on top of the rules' filters", [this]()
	{
		// As a component's Accepts Item would: here, nothing goes on the first slot.
		Slots.SetSlotFilter([](int32 SlotIndex, FInventoryItemHandle Item)
		{
			return SlotIndex != 0;
		});
		const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Pockets);
		TestFalse(TEXT("Refused by the filter"), Slots.Assign(*Registry, OwnerRoots(), Rules, 0, Knife));
		TestTrue(TEXT("Any slot takes it"), Slots.CanAssign(*Registry, OwnerRoots(), Rules, Knife, INDEX_NONE));
		TestEqual(TEXT("The first free slot the filter allows"), Slots.AssignToFreeSlot(*Registry, OwnerRoots(), Rules, Knife), 1);
	});

	It("follow an item as it moves, and empty when it leaves", [this]()
	{
		const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Pockets);
		Slots.Assign(*Registry, OwnerRoots(), Rules, 0, Knife);

		Registry->MoveItem(Knife, Pockets, FInventoryPlacement::AtCell(FIntPoint(3, 0)));
		TestFalse(TEXT("Nothing changed for a move inside"), RefreshSlots());
		TestTrue(TEXT("Still on the slot"), Slots.Get(0) == Knife);

		Registry->MoveItem(Knife, Elsewhere);
		TestTrue(TEXT("Changed as it left"), RefreshSlots());
		TestFalse(TEXT("The slot emptied"), Slots.Get(0).IsSet());
	});

	It("point at another stack of the same kind when its stack goes", [this]()
	{
		// Herbs stack to 5: a full stack, then a second one.
		const FInventoryItemHandle First = AddTestItem(*Registry, Data->Herb, Pockets, 5);
		const FInventoryItemHandle Second = AddTestItem(*Registry, Data->Herb, Pockets, 3);
		TestTrue(TEXT("Two stacks"), First.IsSet() && Second.IsSet() && First != Second);
		Slots.Assign(*Registry, OwnerRoots(), Rules, 0, First);

		Registry->RemoveItem(First);
		TestTrue(TEXT("Changed"), RefreshSlots());
		TestTrue(TEXT("The other stack"), Slots.Get(0) == Second);

		Rules.bFollowSameKind = false;
		Registry->RemoveItem(Second);
		RefreshSlots();
		TestFalse(TEXT("Emptied without another"), Slots.Get(0).IsSet());
	});

	It("give new finds a free slot, unless their kind has one", [this]()
	{
		Rules.bAutoAssignNewItems = true;
		Rules.AutoAssignFilter.Definitions = { Data->Knife, Data->Rifle, Data->Herb };
		RefreshSlots();

		const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Pockets);
		TestTrue(TEXT("Changed"), RefreshSlots());
		TestEqual(TEXT("The knife on the first slot"), Slots.Find(Knife), 0);

		const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Pockets);
		RefreshSlots();
		TestEqual(TEXT("Not the stone, which the filter leaves out"), Slots.Find(Stone), static_cast<int32>(INDEX_NONE));

		const FInventoryItemHandle Herbs = AddTestItem(*Registry, Data->Herb, Pockets, 4);
		RefreshSlots();
		TestEqual(TEXT("The herbs on the second slot"), Slots.Find(Herbs), 1);

		FInventoryItemHandle Half;
		Registry->SplitStack(Herbs, 2, FInventoryContainerHandle(), {}, &Half);
		RefreshSlots();
		TestEqual(TEXT("Not the split-off herbs, as herbs have a slot"), Slots.Find(Half), static_cast<int32>(INDEX_NONE));
	});

	It("swap slots when each takes the other's item", [this]()
	{
		const FInventoryItemHandle Knife = AddTestItem(*Registry, Data->Knife, Pockets);
		const FInventoryItemHandle Stone = AddTestItem(*Registry, Data->Stone, Pockets);
		Slots.Assign(*Registry, OwnerRoots(), Rules, 0, Knife);
		Slots.Assign(*Registry, OwnerRoots(), Rules, 1, Stone);

		TestTrue(TEXT("Swapped"), Slots.Swap(*Registry, Rules, 0, 1));
		TestTrue(TEXT("Stone first"), Slots.Get(0) == Stone && Slots.Get(1) == Knife);

		Rules.Slots[1].Filter.Definitions.Add(Data->Knife);
		TestFalse(TEXT("The stone doesn't suit the knife slot"), Slots.Swap(*Registry, Rules, 0, 1));
		TestTrue(TEXT("With an empty slot"), Slots.Swap(*Registry, Rules, 0, 2));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
