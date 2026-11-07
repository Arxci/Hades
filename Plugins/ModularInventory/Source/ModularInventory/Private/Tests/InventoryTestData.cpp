#include "Tests/InventoryTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"
#include "Data/InventoryStackRules.h"

namespace InventoryTests
{
	UE_DEFINE_GAMEPLAY_TAG(Slot_MainHand, "Inventory.Test.Slot.MainHand");
	UE_DEFINE_GAMEPLAY_TAG(Slot_OffHand, "Inventory.Test.Slot.OffHand");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Durability, "Inventory.Test.Stat.Durability");
	UE_DEFINE_GAMEPLAY_TAG(Stat_Fire, "Inventory.Test.Stat.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Type_Herb, "Inventory.Test.Type.Herb");
	UE_DEFINE_GAMEPLAY_TAG(Container_Main, "Inventory.Test.Container.Main");

	FTestData::FTestData()
	{
		Grid4x4 = MakeGrid(FIntPoint(4, 4));
		Grid4x2 = MakeGrid(FIntPoint(4, 2));
		Grid2x2 = MakeGrid(FIntPoint(2, 2));
		Grid1x1 = MakeGrid(FIntPoint(1, 1));

		Hands = Make<UInventoryContainerLayout>();
		Hands->Type = EInventoryContainerType::Slots;
		Hands->bEquipment = true;
		for (const FGameplayTag& SlotTag : { FGameplayTag(Slot_MainHand), FGameplayTag(Slot_OffHand) })
		{
			FInventorySlotDefinition& Slot = Hands->Slots.AddDefaulted_GetRef();
			Slot.Tag = SlotTag;
		}

		SmallList = Make<UInventoryContainerLayout>();
		SmallList->Type = EInventoryContainerType::List;
		SmallList->Capacity = 2;

		auto MakeStackRules = [this](int32 MaxStackSize)
		{
			UInventoryStackRules* Rules = Make<UInventoryStackRules>();
			Rules->MaxStackSize = MaxStackSize;
			return Rules;
		};

		Stone = Make<UInventoryItemDefinition>();
		Medkit = Make<UInventoryItemDefinition>();

		Ammo = Make<UInventoryItemDefinition>();
		Ammo->StackRules = MakeStackRules(30);

		Herb = Make<UInventoryItemDefinition>();
		Herb->StackRules = MakeStackRules(5);
		Herb->Tags.AddTag(Type_Herb);

		Knife = Make<UInventoryItemDefinition>();
		Knife->Size = FIntPoint(1, 2);
		Knife->TagValues.Emplace(Stat_Durability, 100.0f);
		Knife->EquipRules = Make<UInventoryEquipRules>();
		Knife->EquipRules->Slots.AddTag(Slot_MainHand);
		Knife->EquipRules->Slots.AddTag(Slot_OffHand);

		Rifle = Make<UInventoryItemDefinition>();
		Rifle->Size = FIntPoint(1, 3);
		Rifle->EquipRules = Make<UInventoryEquipRules>();
		Rifle->EquipRules->Slots.AddTag(Slot_MainHand);
		Rifle->EquipRules->BlockedSlots.AddTag(Slot_OffHand);

		Backpack = Make<UInventoryItemDefinition>();
		Backpack->Size = FIntPoint(2, 2);
		FInventoryItemContainer& BackpackStorage = Backpack->Containers.AddDefaulted_GetRef();
		BackpackStorage.Tag = Container_Main;
		BackpackStorage.Layout = Grid2x2;

		Torch = Make<UInventoryItemDefinition>();
		FInventoryItemSocket& Socket = Torch->Sockets.AddDefaulted_GetRef();
		Socket.Cells.Add(FIntPoint(0, -1));
		Socket.ValueModifiers.Emplace(Stat_Fire, 5.0f);
	}

	UInventoryContainerLayout* FTestData::MakeGrid(FIntPoint Size)
	{
		UInventoryContainerLayout* Layout = Make<UInventoryContainerLayout>();
		Layout->Type = EInventoryContainerType::Grid;
		Layout->GridSize = Size;
		return Layout;
	}

	FInventoryItemHandle AddTestItem(FInventoryRegistry& Registry, UInventoryItemDefinition* Definition, FInventoryContainerHandle Container, int32 Count, const TOptional<FInventoryPlacement>& Placement)
	{
		TArray<FInventoryItemHandle> Added;
		const EInventoryError Error = Registry.AddItem(FInventoryItemSpec(Definition, Count), Container, Placement, &Added);
		return Error == EInventoryError::None && !Added.IsEmpty() ? Added.Last() : FInventoryItemHandle();
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
