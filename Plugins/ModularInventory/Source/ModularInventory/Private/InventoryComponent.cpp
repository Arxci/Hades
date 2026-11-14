#include "InventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Algo/AllOf.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Actor.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "Loot/InventoryLootTable.h"
#include "ModularInventory.h"

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
			RootContainers.Add(Registry.CreateContainer(Config.Layout, Config.Tag, this));
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
	for (const FInventoryContainerHandle& Container : Changes.Containers)
	{
		if (RootContainers.Contains(Container) || OwnsContainer(Container))
		{
			RefreshCosmeticsPreload();
			OnInventoryChanged.Broadcast(this);
			return;
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
	return InventorySubsystem ? InventorySubsystem->GetRegistry().AddItemToContainers(Spec, GetStorageContainers(), bAllowPartial, &OutItems) : 0;
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
		Registry.AddItemToContainers(Spec, Storage, bAllowPartial, nullptr, &Error);
		return Error;
	}) == EInventoryError::None;
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
	return Registry.RunTransaction([&]()
	{
		for (const FInventoryItemSpec& Spec : Rolled)
		{
			EInventoryError Error = EInventoryError::None;
			Registry.AddItemToContainers(Spec, Storage, bAllowPartial, nullptr, &Error);
			if (Error != EInventoryError::None && !bAllowPartial)
			{
				return Error;
			}
		}
		return EInventoryError::None;
	}) == EInventoryError::None;
}

bool UInventoryComponent::TakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = GetSubsystem();
	OutError = InventorySubsystem
		? InventorySubsystem->GetRegistry().MoveItemToContainers(Item, GetStorageContainers(), Count, bAllowPartial)
		: EInventoryError::InvalidContainer;
	return OutError == EInventoryError::None;
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
	return Registry.DryRun([&]() { return Registry.MoveItemToContainers(Item, Storage, Count, bAllowPartial); }) == EInventoryError::None;
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
	Registry.RunTransaction([&]()
	{
		for (const FInventoryItemHandle& Item : Contents)
		{
			Registry.MoveItemToContainers(Item, Storage, 0, true);
		}
		return EInventoryError::None;
	});

	const FInventoryContainerRecord* Emptied = Registry.FindContainer(Container);
	return Emptied && Emptied->Items.IsEmpty();
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
