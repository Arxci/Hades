#include "InventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Algo/AllOf.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Actor.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryCapacityUpgrade.h"
#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Loot/InventoryLootTable.h"
#include "ModularInventory.h"

namespace
{
	/** A spec's value of Tag for one of the item: its own, else its definition's. */
	float GetSpecTagValue(const FInventoryItemSpec& Spec, const FGameplayTag& Tag)
	{
		if (const FInventoryTagValue* Own = Spec.TagValues.FindByPredicate([&Tag](const FInventoryTagValue& TagValue) { return TagValue.Tag == Tag; }))
		{
			return Own->Value;
		}
		const FInventoryTagValue* Default = Spec.Definition ? Spec.Definition->FindTagValue(Tag) : nullptr;
		return Default ? Default->Value : 0.0f;
	}
}

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UInventoryComponent* UInventoryComponent::FindInventoryComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = UInventorySubsystem::Get(this);
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!InventorySubsystem)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Inventory] %s found no inventory subsystem; it has no containers."), *GetPathNameSafe(this));
		return;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	for (const FInventoryContainerConfig& Config : Containers)
	{
		if (Config.Layout)
		{
			const int32 StartingCapacity = Config.bOverrideStartingCapacity ? FMath::Max(0, Config.StartingCapacity) : INDEX_NONE;
			RootContainers.Add(Registry.CreateContainer(Config.Layout, Config.Tag, this, StartingCapacity));
		}
	}
	if (RootContainers.IsEmpty())
	{
		UE_LOG(LogInventory, Warning, TEXT("[Inventory] %s has no containers with a layout, so it can't hold anything."), *GetPathNameSafe(this));
	}
	RegistryChangedHandle = Registry.OnChanged.AddUObject(this, &ThisClass::HandleRegistryChanged);

	for (const FInventoryStartingEquipment& Equipment : StartingEquipment)
	{
		EInventoryError Error = EInventoryError::OutOfBounds;
		for (const FInventoryContainerHandle& Container : GetEquipmentContainers())
		{
			const int32 SlotIndex = InventorySubsystem->GetContainerLayout(Container)->FindSlot(Equipment.Slot);
			if (SlotIndex != INDEX_NONE)
			{
				Error = Registry.AddItem(Equipment.Item, Container, FInventoryPlacement::AtIndex(SlotIndex));
				break;
			}
		}
		if (Error != EInventoryError::None)
		{
			UE_LOG(LogInventory, Warning, TEXT("[Inventory] %s couldn't equip its starting %s in %s: %s."), *GetPathNameSafe(this), *GetNameSafe(Equipment.Item.Definition),
				*Equipment.Slot.ToString(), *UEnum::GetValueAsString(Error));
		}
	}

	TArray<FInventoryItemHandle> Added;
	for (const FInventoryItemSpec& Spec : StartingItems)
	{
		if (AddItem(Spec, true, Added) < Spec.Count)
		{
			UE_LOG(LogInventory, Warning, TEXT("[Inventory] %s had no room for all of its starting %s."), *GetPathNameSafe(this), *GetNameSafe(Spec.Definition));
		}
	}

	if (StartingLoot)
	{
		AddLoot(StartingLoot, StartingLootContext, true);
	}
	RefreshCosmeticsPreload();

	// Where it starts, without telling anyone: listeners ask Is Over Carry Limit when they bind.
	bWasOverCarryLimit = IsOverCarryLimit();
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UInventorySubsystem* InventorySubsystem = GetSubsystem())
	{
		FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
		for (const FInventoryContainerHandle& Container : RootContainers)
		{
			Registry.DestroyContainer(Container);
		}
		Registry.OnChanged.Remove(RegistryChangedHandle);
	}
	RootContainers.Reset();
	CosmeticsHandle.Reset();
	PreloadedCosmetics.Reset();

	Super::EndPlay(EndPlayReason);
}

void UInventoryComponent::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	bool bTouched = false;
	for (const FInventoryContainerHandle& Container : Changes.Containers)
	{
		if (RootContainers.Contains(Container) || OwnsContainer(Container))
		{
			bTouched = true;
			RefreshCosmeticsPreload();
			OnInventoryChanged.Broadcast(this);
			break;
		}
	}

	// What it carries can also change with an item's value alone, such as a weight set at runtime, so its own items count too.
	if (HasCarryLimit())
	{
		for (const FInventoryItemHandle& Item : Changes.Items)
		{
			if (bTouched)
			{
				break;
			}
			bTouched = OwnsItem(Item);
		}
		if (bTouched)
		{
			RefreshCarryLimitState();
		}
	}
}

void UInventoryComponent::RefreshCosmeticsPreload()
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!bPreloadCosmetics || !InventorySubsystem)
	{
		return;
	}

	TArray<FSoftObjectPath> Gathered;
	for (const FInventoryItemHandle& Item : FindItems(FInventoryItemQuery()))
	{
		const UInventoryItemDefinition* Definition = InventorySubsystem->GetItemDefinition(Item);
		if (Definition && Definition->Cosmetics)
		{
			Definition->Cosmetics->GatherAllAssets(Gathered);
		}
	}

	TSet<FSoftObjectPath> Wanted;
	Wanted.Append(Gathered);
	if (Wanted.Num() == PreloadedCosmetics.Num() && Wanted.Includes(PreloadedCosmetics))
	{
		return;
	}

	// The new request starts before the old handle goes away, so assets in both stay loaded.
	PreloadedCosmetics = MoveTemp(Wanted);
	CosmeticsHandle = PreloadedCosmetics.IsEmpty() ? nullptr : UAssetManager::GetStreamableManager().RequestAsyncLoad(PreloadedCosmetics.Array(), FStreamableDelegate());
}

// Containers
FInventoryContainerHandle UInventoryComponent::GetContainer(FGameplayTag Tag) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	for (const FInventoryContainerHandle& Container : RootContainers)
	{
		if (InventorySubsystem && InventorySubsystem->GetContainerTag(Container) == Tag)
		{
			return Container;
		}
	}
	return FInventoryContainerHandle();
}

TArray<FInventoryContainerHandle> UInventoryComponent::GetStorageContainers() const
{
	TArray<FInventoryContainerHandle> Storage;
	if (const UInventorySubsystem* InventorySubsystem = GetSubsystem())
	{
		const FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
		Registry.CollectContainers(RootContainers, Storage);
		Storage.RemoveAll([&Registry](const FInventoryContainerHandle& Container)
		{
			return Registry.FindContainer(Container)->Layout->bEquipment;
		});
	}
	return Storage;
}

TArray<FInventoryContainerHandle> UInventoryComponent::GetEquipmentContainers() const
{
	TArray<FInventoryContainerHandle> Equipment;
	if (const UInventorySubsystem* InventorySubsystem = GetSubsystem())
	{
		const FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
		for (const FInventoryContainerHandle& Container : RootContainers)
		{
			const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
			if (Record && Record->Layout->bEquipment)
			{
				Equipment.Add(Container);
			}
		}
	}
	return Equipment;
}

bool UInventoryComponent::OwnsContainer(FInventoryContainerHandle Container) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return InventorySubsystem && RootContainers.Contains(InventorySubsystem->GetRegistry().GetRootContainer(Container));
}

bool UInventoryComponent::OwnsItem(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return InventorySubsystem && OwnsContainer(InventorySubsystem->GetItemContainer(Item));
}

// Items
int32 UInventoryComponent::AddItem(const FInventoryItemSpec& Spec, bool bAllowPartial, TArray<FInventoryItemHandle>& OutItems)
{
	OutItems.Reset();
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!InventorySubsystem)
	{
		return 0;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	TArray<FAppliedUpgrade> Applied;
	int32 Added = 0;
	const EInventoryError Error = Registry.RunTransaction([&]()
	{
		EInventoryError AddError = EInventoryError::None;
		Added = AddToStorage(Registry, Spec, Storage, bAllowPartial, &OutItems, AddError, Applied);
		return AddError;
	});
	if (Error != EInventoryError::None)
	{
		// What was added on the way was undone.
		OutItems.Reset();
		return 0;
	}

	AnnounceUpgrades(Applied);
	return Added;
}

bool UInventoryComponent::CanAddItem(const FInventoryItemSpec& Spec, bool bAllowPartial) const
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!InventorySubsystem)
	{
		return false;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	return Registry.DryRun([&]()
	{
		EInventoryError Error = EInventoryError::None;
		TArray<FAppliedUpgrade> Applied;
		AddToStorage(Registry, Spec, Storage, bAllowPartial, nullptr, Error, Applied);
		return Error;
	}) == EInventoryError::None;
}

int32 UInventoryComponent::AddItemByDefinition(UInventoryItemDefinition* Definition, int32 Count, bool bAllowPartial)
{
	TArray<FInventoryItemHandle> Added;
	return AddItem(FInventoryItemSpec(Definition, Count), bAllowPartial, Added);
}

bool UInventoryComponent::AddLoot(const UInventoryLootTable* LootTable, const FGameplayTagContainer& ContextTags, bool bAllowPartial)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!LootTable || !InventorySubsystem)
	{
		return false;
	}

	TArray<FInventoryItemSpec> Rolled;
	FRandomStream Stream(FMath::Rand());
	LootTable->Roll(Stream, GatherContextTags(ContextTags), Rolled);

	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	TArray<FAppliedUpgrade> Applied;
	const bool bAdded = Registry.RunTransaction([&]()
	{
		for (const FInventoryItemSpec& Spec : Rolled)
		{
			EInventoryError Error = EInventoryError::None;
			AddToStorage(Registry, Spec, Storage, bAllowPartial, nullptr, Error, Applied);
			if (Error != EInventoryError::None && !bAllowPartial)
			{
				return Error;
			}
		}
		return EInventoryError::None;
	}) == EInventoryError::None;

	if (bAdded)
	{
		AnnounceUpgrades(Applied);
	}
	return bAdded;
}

bool UInventoryComponent::TakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!InventorySubsystem)
	{
		OutError = EInventoryError::InvalidContainer;
		return false;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	TArray<FAppliedUpgrade> Applied;
	OutError = Registry.RunTransaction([&]() { return TakeIntoStorage(Registry, Item, Count, bAllowPartial, Storage, Applied); });
	if (OutError != EInventoryError::None)
	{
		return false;
	}

	AnnounceUpgrades(Applied);
	return true;
}

bool UInventoryComponent::CanTakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial) const
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!InventorySubsystem)
	{
		return false;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	return Registry.DryRun([&]()
	{
		TArray<FAppliedUpgrade> Applied;
		return TakeIntoStorage(Registry, Item, Count, bAllowPartial, Storage, Applied);
	}) == EInventoryError::None;
}

bool UInventoryComponent::TransferItem(FInventoryItemHandle Item, UInventoryComponent* Target, int32 Count, bool bAllowPartial, EInventoryError& OutError)
{
	if (!Target)
	{
		OutError = EInventoryError::InvalidContainer;
		return false;
	}
	return Target->TakeItem(Item, Count, bAllowPartial, OutError);
}

bool UInventoryComponent::TakeAll(FInventoryContainerHandle Container)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const FInventoryContainerRecord* Record = InventorySubsystem ? InventorySubsystem->GetRegistry().FindContainer(Container) : nullptr;
	if (!Record)
	{
		return false;
	}

	// One change for listeners, however many items move.
	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	const TArray<FInventoryItemHandle> Contents = Record->Items;
	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	TArray<FAppliedUpgrade> Applied;
	Registry.RunTransaction([&]()
	{
		for (const FInventoryItemHandle& Item : Contents)
		{
			TakeIntoStorage(Registry, Item, 0, true, Storage, Applied);
		}
		return EInventoryError::None;
	});
	AnnounceUpgrades(Applied);

	const FInventoryContainerRecord* Emptied = Registry.FindContainer(Container);
	return Emptied && Emptied->Items.IsEmpty();
}

// Capacity
int32 UInventoryComponent::GetContainerCapacity(FGameplayTag ContainerTag) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const FInventoryContainerHandle Container = GetContainer(ContainerTag);
	return InventorySubsystem && Container.IsSet() ? InventorySubsystem->GetCapacity(Container) : 0;
}

int32 UInventoryComponent::GetContainerMaxCapacity(FGameplayTag ContainerTag) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const FInventoryContainerHandle Container = GetContainer(ContainerTag);
	return InventorySubsystem && Container.IsSet() ? InventorySubsystem->GetMaxCapacity(Container) : 0;
}

bool UInventoryComponent::AddContainerCapacity(FGameplayTag ContainerTag, int32 Count, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const FInventoryContainerHandle Container = GetContainer(ContainerTag);
	OutError = InventorySubsystem && Container.IsSet() ? InventorySubsystem->GetRegistry().AddCapacity(Container, Count) : EInventoryError::InvalidContainer;
	return OutError == EInventoryError::None;
}

bool UInventoryComponent::SetContainerCapacity(FGameplayTag ContainerTag, int32 Count, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const FInventoryContainerHandle Container = GetContainer(ContainerTag);
	OutError = InventorySubsystem && Container.IsSet() ? InventorySubsystem->GetRegistry().SetCapacity(Container, Count) : EInventoryError::InvalidContainer;
	return OutError == EInventoryError::None;
}

bool UInventoryComponent::AddContainerStackBonus(FGameplayTag ContainerTag, const FInventoryItemQuery& ItemQuery, int32 Amount, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const FInventoryContainerHandle Container = GetContainer(ContainerTag);
	OutError = InventorySubsystem && Container.IsSet() ? InventorySubsystem->GetRegistry().AddStackBonus(Container, ItemQuery, Amount) : EInventoryError::InvalidContainer;
	return OutError == EInventoryError::None;
}

// Carry limit
float UInventoryComponent::GetCarried() const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return HasCarryLimit() && InventorySubsystem ? InventorySubsystem->GetRegistry().GetTotalTagValue(RootContainers, CarriedValueTag, true) : 0.0f;
}

void UInventoryComponent::SetMaxCarried(float NewMaxCarried)
{
	MaxCarried = FMath::Max(0.0f, NewMaxCarried);
	RefreshCarryLimitState();
}

float UInventoryComponent::GetCarryRoom() const
{
	return HasCarryLimit() ? FMath::Max(0.0f, MaxCarried - GetCarried()) : TNumericLimits<float>::Max();
}

bool UInventoryComponent::IsOverCarryLimit() const
{
	return HasCarryLimit() && GetCarried() > MaxCarried + KINDA_SMALL_NUMBER;
}

EInventoryError UInventoryComponent::CheckCarryLimit(const FInventoryRegistry& Registry, const FInventoryItemRecord& Item) const
{
	if (CarryLimit != EInventoryCarryLimit::Refuse || !CarriedValueTag.IsValid())
	{
		return EInventoryError::None;
	}

	// Moving things around inside what it already holds changes nothing, and nor does an item that weighs nothing.
	if (Item.Container.IsSet() && RootContainers.Contains(Registry.GetRootContainer(Item.Container)))
	{
		return EInventoryError::None;
	}
	const float Added = Registry.GetCarriedValue(Item, CarriedValueTag);
	if (Added <= 0.0f)
	{
		return EInventoryError::None;
	}

	const float Carried = Registry.GetTotalTagValue(RootContainers, CarriedValueTag, true);
	return Carried + Added > MaxCarried + KINDA_SMALL_NUMBER ? EInventoryError::OverCarryLimit : EInventoryError::None;
}

int32 UInventoryComponent::GetCountItCanCarry(float ValuePerOne, int32 Wanted) const
{
	if (CarryLimit != EInventoryCarryLimit::Refuse || !CarriedValueTag.IsValid() || ValuePerOne <= 0.0f || Wanted <= 0)
	{
		return Wanted;
	}

	const double Room = double(MaxCarried) - double(GetCarried()) + KINDA_SMALL_NUMBER;
	const double Fits = Room > 0.0 ? FMath::FloorToDouble(Room / ValuePerOne) : 0.0;
	return static_cast<int32>(FMath::Clamp(Fits, 0.0, double(Wanted)));
}

void UInventoryComponent::RefreshCarryLimitState()
{
	const bool bIsOver = IsOverCarryLimit();
	if (bIsOver != bWasOverCarryLimit)
	{
		bWasOverCarryLimit = bIsOver;
		OnOverCarryLimitChanged.Broadcast(this, bIsOver);
	}
}

// Capacity upgrades
bool UInventoryComponent::ApplyCapacityUpgrade(FInventoryItemHandle Item, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	const FInventoryCapacityUpgrade* Upgrade = Definition ? Definition->FindFragment<FInventoryCapacityUpgrade>() : nullptr;
	const FInventoryContainerHandle Container = Upgrade && Upgrade->ContainerTag.IsValid() ? GetContainer(Upgrade->ContainerTag) : FInventoryContainerHandle();
	OutError = !Definition ? EInventoryError::InvalidItem
		: !Upgrade ? EInventoryError::InvalidFragment
		: !Container.IsSet() ? EInventoryError::InvalidContainer
		: EInventoryError::None;
	if (OutError != EInventoryError::None)
	{
		return false;
	}

	// One of the stack, used up only once it's applied.
	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	const FGameplayTag ContainerTag = Upgrade->ContainerTag;
	OutError = Registry.RunTransaction([&]() -> EInventoryError
	{
		EInventoryError Error = EInventoryError::None;
		ApplyUpgrade(Registry, *Upgrade, Container, 1, false, Error);
		if (Error == EInventoryError::None)
		{
			Registry.RemoveItem(Item, 1);
		}
		return Error;
	});
	if (OutError != EInventoryError::None)
	{
		return false;
	}

	TArray<FAppliedUpgrade> Applied;
	FAppliedUpgrade& Entry = Applied.AddDefaulted_GetRef();
	Entry.Definition = Definition;
	Entry.ContainerTag = ContainerTag;
	Entry.Times = 1;
	AnnounceUpgrades(Applied);
	return true;
}

bool UInventoryComponent::CanApplyCapacityUpgrade(FInventoryItemHandle Item) const
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	const FInventoryCapacityUpgrade* Upgrade = Definition ? Definition->FindFragment<FInventoryCapacityUpgrade>() : nullptr;
	const FInventoryContainerHandle Container = Upgrade && Upgrade->ContainerTag.IsValid() ? GetContainer(Upgrade->ContainerTag) : FInventoryContainerHandle();
	if (!Container.IsSet())
	{
		return false;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	return Registry.DryRun([&]()
	{
		EInventoryError Error = EInventoryError::None;
		ApplyUpgrade(Registry, *Upgrade, Container, 1, false, Error);
		return Error;
	}) == EInventoryError::None;
}

const FInventoryCapacityUpgrade* UInventoryComponent::FindUpgradeAppliedWhenTaken(const UInventoryItemDefinition* Definition, FInventoryContainerHandle& OutContainer) const
{
	// Only components with the container apply it, so a chest or a pickup keeps an upgrade like any other item.
	const FInventoryCapacityUpgrade* Upgrade = Definition ? Definition->FindFragment<FInventoryCapacityUpgrade>() : nullptr;
	const bool bApplies = Upgrade && Upgrade->bApplyWhenTaken && Upgrade->ContainerTag.IsValid();
	OutContainer = bApplies ? GetContainer(Upgrade->ContainerTag) : FInventoryContainerHandle();

	// Room can't be added to a container without a limit, such as a grid growing to fit, so there the upgrade is kept like any item,
	// rather than turned away for good.
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const bool bNoLimit = OutContainer.IsSet() && Upgrade->Kind == EInventoryCapacityUpgradeKind::Room && InventorySubsystem
		&& InventorySubsystem->GetCapacity(OutContainer) == INDEX_NONE;
	if (bNoLimit)
	{
		OutContainer = FInventoryContainerHandle();
	}
	return OutContainer.IsSet() ? Upgrade : nullptr;
}

int32 UInventoryComponent::ApplyUpgrade(FInventoryRegistry& Registry, const FInventoryCapacityUpgrade& Upgrade, FInventoryContainerHandle Container, int32 Times,
	bool bAllowPartial, EInventoryError& OutError)
{
	const int32 Amount = FMath::Max(1, Upgrade.Amount);
	int32 Applied = 0;
	OutError = Registry.RunTransaction([&]() -> EInventoryError
	{
		for (; Applied < Times; ++Applied)
		{
			// Room stops once the container can't grow any more, with At Max Capacity. Stacks can always grow.
			const EInventoryError Error = Upgrade.Kind == EInventoryCapacityUpgradeKind::StackSize
				? Registry.AddStackBonus(Container, Upgrade.StackItems, Amount)
				: Registry.AddCapacity(Container, Amount);
			if (Error != EInventoryError::None)
			{
				return bAllowPartial && Applied > 0 ? EInventoryError::None : Error;
			}
		}
		return EInventoryError::None;
	});
	return OutError == EInventoryError::None ? Applied : 0;
}

int32 UInventoryComponent::AddToStorage(FInventoryRegistry& Registry, const FInventoryItemSpec& Spec, TConstArrayView<FInventoryContainerHandle> Storage, bool bAllowPartial,
	TArray<FInventoryItemHandle>* OutItems, EInventoryError& OutError, TArray<FAppliedUpgrade>& OutApplied) const
{
	FInventoryContainerHandle UpgradeContainer;
	const FInventoryCapacityUpgrade* Upgrade = FindUpgradeAppliedWhenTaken(Spec.Definition.Get(), UpgradeContainer);
	if (!Upgrade)
	{
		// Past a Refuse carry limit, a partial add takes as many as can be carried and leaves the rest out, as it does what there's no room
		// for; otherwise it fails with Over Carry Limit. The registry's entry check stands behind this for every other way in.
		if (HasCarryLimit() && Spec.Definition && Spec.Count > 0)
		{
			const int32 Carriable = GetCountItCanCarry(GetSpecTagValue(Spec, CarriedValueTag), Spec.Count);
			if (Carriable < Spec.Count)
			{
				if (!bAllowPartial || Carriable < 1)
				{
					OutError = EInventoryError::OverCarryLimit;
					return 0;
				}
				FInventoryItemSpec CarriableSpec = Spec;
				CarriableSpec.Count = Carriable;
				return Registry.AddItemToContainers(CarriableSpec, Storage, bAllowPartial, OutItems, &OutError);
			}
		}
		return Registry.AddItemToContainers(Spec, Storage, bAllowPartial, OutItems, &OutError);
	}
	if (Spec.Count < 1)
	{
		OutError = EInventoryError::InvalidCount;
		return 0;
	}

	// Applied rather than stored, so it needs no room.
	const int32 Applied = ApplyUpgrade(Registry, *Upgrade, UpgradeContainer, Spec.Count, bAllowPartial, OutError);
	if (Applied > 0)
	{
		FAppliedUpgrade& Entry = OutApplied.AddDefaulted_GetRef();
		Entry.Definition = Spec.Definition.Get();
		Entry.ContainerTag = Upgrade->ContainerTag;
		Entry.Times = Applied;
	}
	return Applied;
}

EInventoryError UInventoryComponent::TakeIntoStorage(FInventoryRegistry& Registry, FInventoryItemHandle Item, int32 Count, bool bAllowPartial,
	TConstArrayView<FInventoryContainerHandle> Storage, TArray<FAppliedUpgrade>& OutApplied) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	FInventoryContainerHandle UpgradeContainer;
	const FInventoryCapacityUpgrade* Upgrade = Record ? FindUpgradeAppliedWhenTaken(Record->Definition.Get(), UpgradeContainer) : nullptr;
	if (!Upgrade)
	{
		// Past a Refuse carry limit, a partial take carries as much of the stack as it can, and leaves the rest; otherwise it fails with
		// Over Carry Limit. Nothing changes for an item already held here. An item with things inside it goes whole or not at all, which
		// the registry's entry check decides.
		int32 CountToTake = Count;
		const bool bHeldHere = Record && Record->Container.IsSet() && RootContainers.Contains(Registry.GetRootContainer(Record->Container));
		if (HasCarryLimit() && Record && !bHeldHere && Record->Containers.IsEmpty() && Count >= 0 && Count <= Record->Count)
		{
			const int32 Wanted = Count > 0 ? Count : Record->Count;
			const int32 Carriable = GetCountItCanCarry(Registry.GetTagValue(Item, CarriedValueTag), Wanted);
			if (Carriable < Wanted)
			{
				if (!bAllowPartial || Carriable < 1 || !Record->Definition->CanSplit())
				{
					return EInventoryError::OverCarryLimit;
				}
				CountToTake = Carriable;
			}
		}
		return Registry.MoveItemToContainers(Item, Storage, CountToTake, bAllowPartial);
	}
	if (Count < 0 || Count > Record->Count)
	{
		return EInventoryError::InvalidCount;
	}

	// Used up rather than moved: Count of the stack, or all of it.
	const UInventoryItemDefinition* Definition = Record->Definition.Get();
	const int32 Wanted = Count > 0 ? Count : Record->Count;
	EInventoryError Error = EInventoryError::None;
	const int32 Applied = ApplyUpgrade(Registry, *Upgrade, UpgradeContainer, Wanted, bAllowPartial, Error);
	if (Error != EInventoryError::None)
	{
		return Error;
	}
	Registry.RemoveItem(Item, Applied);

	FAppliedUpgrade& Entry = OutApplied.AddDefaulted_GetRef();
	Entry.Definition = Definition;
	Entry.ContainerTag = Upgrade->ContainerTag;
	Entry.Times = Applied;
	return EInventoryError::None;
}

void UInventoryComponent::AnnounceUpgrades(const TArray<FAppliedUpgrade>& Applied)
{
	for (const FAppliedUpgrade& Upgrade : Applied)
	{
		for (int32 Index = 0; Index < Upgrade.Times; ++Index)
		{
			OnCapacityUpgraded.Broadcast(this, Upgrade.ContainerTag, Upgrade.Definition.Get());
		}
	}
}

int32 UInventoryComponent::RemoveItems(const FInventoryItemQuery& Query, int32 Count)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!InventorySubsystem)
	{
		return 0;
	}

	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	int32 Removed = 0;
	Registry.RunTransaction([&]()
	{
		TArray<FInventoryItemHandle> Found;
		Registry.FindItems(RootContainers, Query, true, Found);
		for (const FInventoryItemHandle& Item : Found)
		{
			const int32 Remaining = Count > 0 ? Count - Removed : 0;
			if (Count > 0 && Remaining <= 0)
			{
				break;
			}
			Removed += Registry.RemoveItem(Item, Remaining);
		}
		return EInventoryError::None;
	});
	return Removed;
}

TArray<FInventoryItemHandle> UInventoryComponent::FindItems(const FInventoryItemQuery& Query) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return InventorySubsystem ? InventorySubsystem->FindItems(RootContainers, Query, true) : TArray<FInventoryItemHandle>();
}

int32 UInventoryComponent::CountItems(const FInventoryItemQuery& Query) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return InventorySubsystem ? InventorySubsystem->CountItems(RootContainers, Query, true) : 0;
}

float UInventoryComponent::GetTotalTagValue(FGameplayTag Tag) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return InventorySubsystem ? InventorySubsystem->GetTotalTagValue(RootContainers, Tag, true) : 0.0f;
}

// Crafting
FGameplayTagContainer UInventoryComponent::GatherContextTags(const FGameplayTagContainer& ExtraTags) const
{
	FGameplayTagContainer Tags = ExtraTags;
	if (const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		FGameplayTagContainer OwnerTags;
		AbilitySystem->GetOwnedGameplayTags(OwnerTags);
		Tags.AppendTags(OwnerTags);
	}
	return Tags;
}

bool UInventoryComponent::CanCraft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	return Recipe && InventorySubsystem
		&& InventorySubsystem->GetRegistry().CanCraft(*Recipe, GetStorageContainers(), GatherContextTags(ContextTags), Times) == EInventoryError::None;
}

bool UInventoryComponent::Craft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError)
{
	OutItems.Reset();
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!Recipe || !InventorySubsystem)
	{
		OutError = EInventoryError::InvalidDefinition;
		return false;
	}

	const TArray<FInventoryContainerHandle> Storage = GetStorageContainers();
	FRandomStream Stream(FMath::Rand());
	OutError = InventorySubsystem->GetRegistry().Craft(*Recipe, Storage, Storage, GatherContextTags(ContextTags), Times, Stream, &OutItems);
	return OutError == EInventoryError::None;
}

TArray<UInventoryRecipe*> UInventoryComponent::GetCraftableRecipes(const UInventoryRecipeBook* Book, const FGameplayTagContainer& ContextTags) const
{
	TArray<UInventoryRecipe*> Craftable;
	if (Book)
	{
		for (UInventoryRecipe* Recipe : Book->GetRecipes())
		{
			if (CanCraft(Recipe, ContextTags))
			{
				Craftable.Add(Recipe);
			}
		}
	}
	return Craftable;
}

TArray<UInventoryRecipe*> UInventoryComponent::GetRecipesUsing(const UInventoryRecipeBook* Book, FInventoryItemHandle Item) const
{
	TArray<UInventoryRecipe*> Using;
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!Book || !InventorySubsystem)
	{
		return Using;
	}

	for (UInventoryRecipe* Recipe : Book->GetRecipes())
	{
		const bool bUsesItem = Recipe && Recipe->Ingredients.ContainsByPredicate([InventorySubsystem, &Item](const FInventoryRecipeIngredient& Ingredient)
		{
			return InventorySubsystem->MatchesQuery(Item, Ingredient.Query);
		});
		if (bUsesItem)
		{
			Using.Add(Recipe);
		}
	}
	return Using;
}

UInventoryRecipe* UInventoryComponent::FindRecipeMadeFrom(const UInventoryRecipeBook* Book, const TArray<FInventoryItemHandle>& Items, const FGameplayTagContainer& ContextTags) const
{
	const UInventorySubsystem* InventorySubsystem = GetSubsystem();
	const bool bOwnsAll = Algo::AllOf(Items, [this](const FInventoryItemHandle& Item) { return OwnsItem(Item); });
	if (!Book || !InventorySubsystem || !bOwnsAll)
	{
		return nullptr;
	}

	const FGameplayTagContainer AllContextTags = GatherContextTags(ContextTags);
	for (UInventoryRecipe* Recipe : Book->GetRecipes())
	{
		if (Recipe && InventorySubsystem->GetRegistry().IsMadeFrom(*Recipe, Items, AllContextTags))
		{
			return Recipe;
		}
	}
	return nullptr;
}

bool UInventoryComponent::CraftFromItems(const UInventoryRecipe* Recipe, const TArray<FInventoryItemHandle>& Items, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError)
{
	OutItems.Reset();
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	if (!Recipe || !InventorySubsystem)
	{
		OutError = EInventoryError::InvalidDefinition;
		return false;
	}
	if (!Algo::AllOf(Items, [this](const FInventoryItemHandle& Item) { return OwnsItem(Item); }))
	{
		OutError = EInventoryError::InvalidItem;
		return false;
	}

	FRandomStream Stream(FMath::Rand());
	OutError = InventorySubsystem->GetRegistry().CraftFromItems(*Recipe, Items, GetStorageContainers(), GatherContextTags(ContextTags), 1, Stream, &OutItems);
	return OutError == EInventoryError::None;
}
