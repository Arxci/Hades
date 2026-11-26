#include "HadesInventoryComponent.h"

#include "Engine/World.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Player/HadesPlayerController.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventorySubsystem.h"

#define LOCTEXT_NAMESPACE "HadesInventory"

void UHadesInventoryComponent::BeginPlay()
{
	// Bound once the starting items are in, so starting with a pouch says nothing.
	Super::BeginPlay();

	OnCapacityUpgraded.AddUniqueDynamic(this, &ThisClass::HandleCapacityUpgraded);
}

void UHadesInventoryComponent::HandleCapacityUpgraded(UInventoryComponent* Inventory, FGameplayTag ContainerTag, const UInventoryItemDefinition* Upgrade)
{
	AHadesPlayerController::ShowMessageTo(GetOwner(), FText::Format(LOCTEXT("InventoryGrew", "{0}: you can carry more."), Upgrade ? Upgrade->DisplayName : FText::GetEmpty()));
}

// Hands
bool UHadesInventoryComponent::DrawItem(FInventoryItemHandle Item)
{
	if (IsInHand(Item))
	{
		return true;
	}

	UInventoryEquipmentComponent* Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwner());
	const FGameplayTag Hand = ChooseHand(Item);
	if (!Equipment || !Hand.IsValid() || !OwnsItem(Item))
	{
		return false;
	}

	StowItem(GetItemInHand());
	EInventoryError Error;
	return Equipment->EquipInPlace(Item, Hand, Error);
}

bool UHadesInventoryComponent::StowItem(FInventoryItemHandle Item)
{
	UInventoryEquipmentComponent* Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwner());
	EInventoryError Error;
	return Equipment && IsInHand(Item) && Equipment->UnequipItem(Item, Error);
}

bool UHadesInventoryComponent::CanDraw(FInventoryItemHandle Item) const
{
	return OwnsItem(Item) && ChooseHand(Item).IsValid();
}

bool UHadesInventoryComponent::IsInHand(FInventoryItemHandle Item) const
{
	return Item.IsSet() && GetItemInHand() == Item;
}

FInventoryItemHandle UHadesInventoryComponent::GetItemInHand() const
{
	const UInventoryEquipmentComponent* Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwner());
	if (!Equipment)
	{
		return FInventoryItemHandle();
	}

	const FInventoryItemHandle Right = Equipment->GetEquippedItem(HadesGameplayTags::Equipment_Slot_Hand_Right);
	return Right.IsSet() ? Right : Equipment->GetEquippedItem(HadesGameplayTags::Equipment_Slot_Hand_Left);
}

FGameplayTag UHadesInventoryComponent::ChooseHand(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	const UInventoryEquipRules* Rules = Definition ? Definition->EquipRules.Get() : nullptr;
	for (const FGameplayTag& Hand : { FGameplayTag(HadesGameplayTags::Equipment_Slot_Hand_Right), FGameplayTag(HadesGameplayTags::Equipment_Slot_Hand_Left) })
	{
		if (Rules && Rules->AllowsSlot(Hand))
		{
			return Hand;
		}
	}
	return FGameplayTag();
}

// Item actions
bool UHadesInventoryComponent::CanDropItem(FInventoryItemHandle Item) const
{
	return Super::CanDropItem(Item) && !IsKeyItem(Item);
}

bool UHadesInventoryComponent::CanCombine(FInventoryItemHandle Item) const
{
	if (!OwnsItem(Item))
	{
		return false;
	}

	const bool bHasGunPartner = FindItems(FInventoryItemQuery()).ContainsByPredicate([&](const FInventoryItemHandle& Other)
	{
		return FindGunLoadedBy(Item, Other).IsSet();
	});
	return bHasGunPartner || !GetRecipesUsing(CombineRecipes, Item).IsEmpty();
}

bool UHadesInventoryComponent::CombineItems(FInventoryItemHandle Item, FInventoryItemHandle Other)
{
	// A gun and its ammo: loaded from the stack the player chose first.
	const FInventoryItemHandle Gun = FindGunLoadedBy(Item, Other);
	if (Gun.IsSet())
	{
		if (LoadGun(Gun, Gun == Item ? Other : Item))
		{
			return true;
		}
		const bool bFull = GetLoadedRounds(Gun) >= GetCapacity(Gun);
		AHadesPlayerController::ShowMessageTo(GetOwner(), bFull ? LOCTEXT("GunFull", "It's already fully loaded.") : LOCTEXT("CantLoad", "You can't load that right now."));
		return false;
	}

	// Made from these two, not others like them, so items with their own state are the ones used up.
	const TArray<FInventoryItemHandle> Ingredients = { Item, Other };
	UInventoryRecipe* Recipe = Item != Other ? FindRecipeMadeFrom(CombineRecipes, Ingredients, FGameplayTagContainer()) : nullptr;
	if (!Recipe)
	{
		AHadesPlayerController::ShowMessageTo(GetOwner(), LOCTEXT("CantCombine", "Those can't be combined."));
		return false;
	}

	TArray<FInventoryItemHandle> Made;
	EInventoryError Error;
	if (CraftFromItems(Recipe, Ingredients, FGameplayTagContainer(), Made, Error))
	{
		return true;
	}
	AHadesPlayerController::ShowMessageTo(GetOwner(), Error == EInventoryError::NoSpace
		? LOCTEXT("NoRoomToCombine", "Not enough space.")
		: LOCTEXT("CantCombineNow", "You can't combine those right now."));
	return false;
}

// Guns
bool UHadesInventoryComponent::IsGun(FInventoryItemHandle Item) const
{
	return FindGun(Item) != nullptr;
}

bool UHadesInventoryComponent::IsAmmoFor(FInventoryItemHandle Gun, FInventoryItemHandle Ammo) const
{
	// An empty Ammo query would match every item, the gun included, so a gun without one loads nothing. Guns never load guns.
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FHadesGun* GunData = FindGun(Gun);
	return InventorySubsystem && GunData && !GunData->Ammo.IsEmpty() && Ammo.IsSet() && Ammo != Gun && !IsGun(Ammo)
		&& InventorySubsystem->MatchesQuery(Ammo, GunData->Ammo);
}

int32 UHadesInventoryComponent::GetLoadedRounds(FInventoryItemHandle Gun) const
{
	// The gun's own once it's been fired or loaded, or else its definition's: what it's found with.
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	return InventorySubsystem && IsGun(Gun) ? FMath::Max(0, FMath::RoundToInt32(InventorySubsystem->GetTagValue(Gun, HadesGameplayTags::Item_Stat_Rounds))) : 0;
}

int32 UHadesInventoryComponent::GetCapacity(FInventoryItemHandle Gun) const
{
	// Effective, so attachments count, as item widgets and details panels read it.
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	return InventorySubsystem && IsGun(Gun) ? FMath::Max(0, FMath::RoundToInt32(InventorySubsystem->GetEffectiveTagValue(Gun, HadesGameplayTags::Item_Stat_Capacity))) : 0;
}

int32 UHadesInventoryComponent::GetSpareRounds(FInventoryItemHandle Gun) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FHadesGun* GunData = FindGun(Gun);
	if (!InventorySubsystem || !GunData || GunData->Ammo.IsEmpty())
	{
		return 0;
	}

	int32 Spare = 0;
	for (const FInventoryItemHandle& Stack : FindItems(GunData->Ammo))
	{
		Spare += IsAmmoFor(Gun, Stack) ? InventorySubsystem->GetItemCount(Stack) : 0;
	}
	return Spare;
}

bool UHadesInventoryComponent::ReloadGun(FInventoryItemHandle Gun)
{
	return LoadGun(Gun, FInventoryItemHandle());
}

bool UHadesInventoryComponent::LoadGun(FInventoryItemHandle Gun, FInventoryItemHandle PreferredAmmo)
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FHadesGun* GunData = FindGun(Gun);
	if (!InventorySubsystem || !GunData || !OwnsItem(Gun))
	{
		return false;
	}

	const int32 Capacity = GetCapacity(Gun);
	if (Capacity <= 0 || GunData->Ammo.IsEmpty())
	{
		UE_LOG(LogHadesInventory, Warning, TEXT("[Inventory] %s can't be loaded: give its definition an Item.Stat.Capacity tag value, and its Hades Gun fragment the Ammo that loads it."),
			*GetNameSafe(InventorySubsystem->GetItemDefinition(Gun)));
		return false;
	}

	const int32 Loaded = GetLoadedRounds(Gun);
	const int32 Wanted = Capacity - Loaded;
	if (Wanted <= 0)
	{
		return false;
	}

	// The stack the player chose first, then any other ammo for it.
	TArray<FInventoryItemHandle> Stacks = FindItems(GunData->Ammo);
	if (IsAmmoFor(Gun, PreferredAmmo) && OwnsItem(PreferredAmmo))
	{
		Stacks.Remove(PreferredAmmo);
		Stacks.Insert(PreferredAmmo, 0);
	}

	// One change: the rounds go in only as the ammo comes out, or neither happens.
	FInventoryRegistry& Registry = InventorySubsystem->GetRegistry();
	int32 Taken = 0;
	const EInventoryError Error = Registry.RunTransaction([&]() -> EInventoryError
	{
		for (const FInventoryItemHandle& Stack : Stacks)
		{
			if (Taken < Wanted && IsAmmoFor(Gun, Stack))
			{
				Taken += Registry.RemoveItem(Stack, FMath::Min(Wanted - Taken, InventorySubsystem->GetItemCount(Stack)));
			}
		}
		return Taken > 0 ? Registry.SetTagValue(Gun, HadesGameplayTags::Item_Stat_Rounds, static_cast<float>(Loaded + Taken)) : EInventoryError::MissingIngredients;
	});
	return Error == EInventoryError::None;
}

bool UHadesInventoryComponent::SpendRounds(FInventoryItemHandle Gun, int32 Count)
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const int32 Loaded = GetLoadedRounds(Gun);
	if (!InventorySubsystem || !IsGun(Gun) || Count <= 0 || Count > Loaded)
	{
		return false;
	}
	return InventorySubsystem->SetTagValue(Gun, HadesGameplayTags::Item_Stat_Rounds, static_cast<float>(Loaded - Count));
}

const FHadesGun* UHadesInventoryComponent::FindGun(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	return Definition ? Definition->FindFragment<FHadesGun>() : nullptr;
}

FInventoryItemHandle UHadesInventoryComponent::FindGunLoadedBy(FInventoryItemHandle Item, FInventoryItemHandle Other) const
{
	return IsAmmoFor(Item, Other) ? Item : IsAmmoFor(Other, Item) ? Other : FInventoryItemHandle();
}

bool UHadesInventoryComponent::IsKeyItem(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	return InventorySubsystem && InventorySubsystem->HasItemTag(Item, HadesGameplayTags::Item_Type_KeyItem);
}

// Organizing
bool UHadesInventoryComponent::OrganizeInventory()
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	EInventoryError Error;
	return InventorySubsystem && InventorySubsystem->SortContainer(GetContainer(HadesGameplayTags::Container_Inventory), Error);
}

#undef LOCTEXT_NAMESPACE
