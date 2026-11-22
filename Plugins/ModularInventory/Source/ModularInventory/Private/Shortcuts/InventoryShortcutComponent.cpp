#include "Shortcuts/InventoryShortcutComponent.h"

#include "GameFramework/Actor.h"

#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventoryRegistry.h"
#include "InventorySubsystem.h"
#include "ModularInventory.h"

// Slots
void FInventoryShortcutSlots::SetNum(int32 NumSlots)
{
	Items.SetNum(FMath::Max(0, NumSlots));
	Kinds.SetNum(Items.Num());
}

FInventoryItemHandle FInventoryShortcutSlots::Get(int32 SlotIndex) const
{
	return Items.IsValidIndex(SlotIndex) ? Items[SlotIndex] : FInventoryItemHandle();
}

int32 FInventoryShortcutSlots::Find(FInventoryItemHandle Item) const
{
	return Item.IsSet() ? Items.IndexOfByKey(Item) : INDEX_NONE;
}

bool FInventoryShortcutSlots::IsHeld(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	return Record && Roots.Contains(Registry.GetRootContainer(Record->Container));
}

bool FInventoryShortcutSlots::SlotTakes(const FInventoryRegistry& Registry, const FInventoryShortcutRules& Rules, int32 SlotIndex, FInventoryItemHandle Item) const
{
	if (!Items.IsValidIndex(SlotIndex) || !Registry.MatchesQuery(Item, Rules.Filter))
	{
		return false;
	}
	return !Rules.Slots.IsValidIndex(SlotIndex) || Registry.MatchesQuery(Item, Rules.Slots[SlotIndex].Filter);
}

bool FInventoryShortcutSlots::HasKindOnSlot(const FInventoryRegistry& Registry, FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	return Record && Kinds.ContainsByPredicate([Record](const TWeakObjectPtr<const UInventoryItemDefinition>& Kind)
	{
		return Kind.IsValid() && Kind.Get() == Record->Definition.Get();
	});
}

void FInventoryShortcutSlots::Set(const FInventoryRegistry& Registry, int32 SlotIndex, FInventoryItemHandle Item)
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	Items[SlotIndex] = Item;
	Kinds[SlotIndex] = Record ? Record->Definition.Get() : nullptr;
}

bool FInventoryShortcutSlots::CanAssign(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules,
	FInventoryItemHandle Item, int32 SlotIndex) const
{
	if (!IsHeld(Registry, Roots, Item))
	{
		return false;
	}
	if (SlotIndex != INDEX_NONE)
	{
		return SlotTakes(Registry, Rules, SlotIndex, Item);
	}
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (SlotTakes(Registry, Rules, Index, Item))
		{
			return true;
		}
	}
	return false;
}

bool FInventoryShortcutSlots::Assign(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules,
	int32 SlotIndex, FInventoryItemHandle Item)
{
	if (!Items.IsValidIndex(SlotIndex) || !CanAssign(Registry, Roots, Rules, Item, SlotIndex))
	{
		return false;
	}

	// An item is on one slot at most, so putting it on another moves it there.
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Index != SlotIndex && Items[Index] == Item)
		{
			Items[Index].Reset();
			Kinds[Index].Reset();
		}
	}
	Set(Registry, SlotIndex, Item);
	return true;
}

int32 FInventoryShortcutSlots::AssignToFreeSlot(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules,
	FInventoryItemHandle Item)
{
	const int32 Existing = Find(Item);
	if (Existing != INDEX_NONE || !IsHeld(Registry, Roots, Item))
	{
		return Existing;
	}

	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (!Items[Index].IsSet() && SlotTakes(Registry, Rules, Index, Item))
		{
			Set(Registry, Index, Item);
			return Index;
		}
	}
	return INDEX_NONE;
}

bool FInventoryShortcutSlots::Clear(int32 SlotIndex)
{
	if (!Items.IsValidIndex(SlotIndex) || !Items[SlotIndex].IsSet())
	{
		return false;
	}
	Items[SlotIndex].Reset();
	Kinds[SlotIndex].Reset();
	return true;
}

bool FInventoryShortcutSlots::Swap(const FInventoryRegistry& Registry, const FInventoryShortcutRules& Rules, int32 SlotA, int32 SlotB)
{
	if (!Items.IsValidIndex(SlotA) || !Items.IsValidIndex(SlotB) || SlotA == SlotB)
	{
		return false;
	}

	// Each item has to suit the slot it moves to. Nothing has to suit an empty slot.
	const FInventoryItemHandle ItemA = Items[SlotA];
	const FInventoryItemHandle ItemB = Items[SlotB];
	const bool bBSuitsA = !ItemB.IsSet() || SlotTakes(Registry, Rules, SlotA, ItemB);
	const bool bASuitsB = !ItemA.IsSet() || SlotTakes(Registry, Rules, SlotB, ItemA);
	if (!bBSuitsA || !bASuitsB)
	{
		return false;
	}

	Items.Swap(SlotA, SlotB);
	Kinds.Swap(SlotA, SlotB);
	return true;
}

bool FInventoryShortcutSlots::Refresh(const FInventoryRegistry& Registry, TConstArrayView<FInventoryContainerHandle> Roots, const FInventoryShortcutRules& Rules)
{
	bool bChanged = false;
	TArray<FInventoryItemHandle> Held;
	Registry.FindItems(Roots, FInventoryItemQuery(), true, Held);

	// A slot whose item is gone or out of reach points at another of the same kind, if it may, or empties.
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (!Items[Index].IsSet() || IsHeld(Registry, Roots, Items[Index]))
		{
			continue;
		}

		FInventoryItemHandle Replacement;
		const UInventoryItemDefinition* Kind = Kinds[Index].Get();
		if (Rules.bFollowSameKind && Kind)
		{
			for (const FInventoryItemHandle& Candidate : Held)
			{
				const FInventoryItemRecord* Record = Registry.FindItem(Candidate);
				if (Record && Record->Definition.Get() == Kind && Find(Candidate) == INDEX_NONE && SlotTakes(Registry, Rules, Index, Candidate))
				{
					Replacement = Candidate;
					break;
				}
			}
		}

		Items[Index] = Replacement;
		if (!Replacement.IsSet())
		{
			Kinds[Index].Reset();
		}
		bChanged = true;
	}

	// New finds go on free slots, unless their kind is on a slot already, such as half of a stack just split off.
	if (Rules.bAutoAssignNewItems)
	{
		for (const FInventoryItemHandle& Item : Held)
		{
			const bool bNew = !Known.Contains(Item) && Find(Item) == INDEX_NONE;
			if (bNew && Registry.MatchesQuery(Item, Rules.AutoAssignFilter) && !HasKindOnSlot(Registry, Item))
			{
				bChanged |= AssignToFreeSlot(Registry, Roots, Rules, Item) != INDEX_NONE;
			}
		}
	}

	Known.Reset();
	Known.Append(Held);
	return bChanged;
}

// Component
UInventoryShortcutComponent::UInventoryShortcutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Rules.Slots.SetNum(4);
}

UInventoryShortcutComponent* UInventoryShortcutComponent::FindShortcutComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UInventoryShortcutComponent>() : nullptr;
}

void UInventoryShortcutComponent::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(Rules.Slots.Num());
	Inventory = UInventoryComponent::FindInventoryComponent(GetOwner());
	if (!Inventory)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Inventory] %s needs an Inventory Component on %s, so its slots stay empty."), *GetPathNameSafe(this), *GetNameSafe(GetOwner()));
		return;
	}
	Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);

	// What the owner already holds counts as found now, so Auto Assign New Items gives it slots too. Whatever its Inventory Component
	// adds as it begins play afterwards arrives as a change.
	Refresh();
}

void UInventoryShortcutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	Inventory = nullptr;

	Super::EndPlay(EndPlayReason);
}

const FInventoryRegistry* UInventoryShortcutComponent::GetRegistry() const
{
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	return Subsystem ? &Subsystem->GetRegistry() : nullptr;
}

TArray<FInventoryContainerHandle> UInventoryShortcutComponent::GetRoots() const
{
	return Inventory ? Inventory->GetContainers() : TArray<FInventoryContainerHandle>();
}

void UInventoryShortcutComponent::HandleInventoryChanged(UInventoryComponent* ChangedInventory)
{
	Refresh();
}

void UInventoryShortcutComponent::Refresh()
{
	const FInventoryRegistry* Registry = GetRegistry();
	if (Registry && Slots.Refresh(*Registry, GetRoots(), Rules))
	{
		OnShortcutsChanged.Broadcast();
	}
}

bool UInventoryShortcutComponent::AssignItem(int32 SlotIndex, FInventoryItemHandle Item)
{
	const FInventoryRegistry* Registry = GetRegistry();
	if (!Registry || !Slots.Assign(*Registry, GetRoots(), Rules, SlotIndex, Item))
	{
		return false;
	}
	OnShortcutsChanged.Broadcast();
	return true;
}

int32 UInventoryShortcutComponent::AssignToFreeSlot(FInventoryItemHandle Item)
{
	const int32 Existing = Slots.Find(Item);
	const FInventoryRegistry* Registry = GetRegistry();
	if (Existing != INDEX_NONE || !Registry)
	{
		return Existing;
	}

	const int32 Assigned = Slots.AssignToFreeSlot(*Registry, GetRoots(), Rules, Item);
	if (Assigned != INDEX_NONE)
	{
		OnShortcutsChanged.Broadcast();
	}
	return Assigned;
}

void UInventoryShortcutComponent::ClearSlot(int32 SlotIndex)
{
	if (Slots.Clear(SlotIndex))
	{
		OnShortcutsChanged.Broadcast();
	}
}

void UInventoryShortcutComponent::ClearAllSlots()
{
	bool bChanged = false;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		bChanged |= Slots.Clear(Index);
	}
	if (bChanged)
	{
		OnShortcutsChanged.Broadcast();
	}
}

bool UInventoryShortcutComponent::SwapSlots(int32 SlotA, int32 SlotB)
{
	const FInventoryRegistry* Registry = GetRegistry();
	if (!Registry || !Slots.Swap(*Registry, Rules, SlotA, SlotB))
	{
		return false;
	}
	OnShortcutsChanged.Broadcast();
	return true;
}

void UInventoryShortcutComponent::ActivateSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= GetNumSlots())
	{
		return;
	}

	const FInventoryItemHandle Item = Slots.Get(SlotIndex);
	NativeOnShortcutActivated(SlotIndex, Item);
	OnShortcutActivated.Broadcast(SlotIndex, Item);
}

void UInventoryShortcutComponent::ActivateSlotWithTag(FGameplayTag SlotTag)
{
	ActivateSlot(FindSlotWithTag(SlotTag));
}

FInventoryItemHandle UInventoryShortcutComponent::GetSlotItem(int32 SlotIndex) const
{
	return Slots.Get(SlotIndex);
}

TArray<FInventoryItemHandle> UInventoryShortcutComponent::GetSlotItems() const
{
	TArray<FInventoryItemHandle> SlotItems;
	SlotItems.Reserve(Slots.Num());
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		SlotItems.Add(Slots.Get(Index));
	}
	return SlotItems;
}

int32 UInventoryShortcutComponent::FindSlotOfItem(FInventoryItemHandle Item) const
{
	return Slots.Find(Item);
}

int32 UInventoryShortcutComponent::FindSlotWithTag(FGameplayTag SlotTag) const
{
	if (!SlotTag.IsValid())
	{
		return INDEX_NONE;
	}
	return Rules.Slots.IndexOfByPredicate([&SlotTag](const FInventoryShortcutSlotDefinition& Slot) { return Slot.Tag == SlotTag; });
}

FGameplayTag UInventoryShortcutComponent::GetSlotTag(int32 SlotIndex) const
{
	return Rules.Slots.IsValidIndex(SlotIndex) ? Rules.Slots[SlotIndex].Tag : FGameplayTag();
}

FText UInventoryShortcutComponent::GetSlotDisplayName(int32 SlotIndex) const
{
	return Rules.Slots.IsValidIndex(SlotIndex) ? Rules.Slots[SlotIndex].DisplayName : FText::GetEmpty();
}

bool UInventoryShortcutComponent::CanAssign(FInventoryItemHandle Item, int32 SlotIndex) const
{
	const FInventoryRegistry* Registry = GetRegistry();
	return Registry && Slots.CanAssign(*Registry, GetRoots(), Rules, Item, SlotIndex);
}
