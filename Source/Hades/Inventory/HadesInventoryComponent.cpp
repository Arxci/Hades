#include "HadesInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameplayEffect.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Inventory/HadesItemPickup.h"
#include "Hades/Player/HadesPlayerController.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentComponent.h"
#include "InventorySubsystem.h"

#define LOCTEXT_NAMESPACE "HadesInventory"

namespace
{
	constexpr double DropDistance = 60.0;
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
bool UHadesInventoryComponent::CanUseItem(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	const FHadesItemUse* Use = Definition ? Definition->FindFragment<FHadesItemUse>() : nullptr;
	return Use && Use->Effect && OwnsItem(Item);
}

bool UHadesInventoryComponent::UseItem(FInventoryItemHandle Item)
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!InventorySubsystem || !AbilitySystem || !CanUseItem(Item))
	{
		return false;
	}

	const FHadesItemUse* Use = InventorySubsystem->GetItemDefinition(Item)->FindFragment<FHadesItemUse>();
	FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Use->Effect, 1.0f, Context);
	if (Spec.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	InventorySubsystem->RemoveItem(Item, 1);
	return true;
}

bool UHadesInventoryComponent::CanDiscardItem(FInventoryItemHandle Item) const
{
	return OwnsItem(Item) && !IsKeyItem(Item);
}

bool UHadesInventoryComponent::DiscardItem(FInventoryItemHandle Item)
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	if (!InventorySubsystem || !CanDiscardItem(Item))
	{
		return false;
	}
	if (!DroppedPickupClass)
	{
		return InventorySubsystem->RemoveItem(Item) > 0;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHadesItemPickup* Pickup = GetWorld()->SpawnActor<AHadesItemPickup>(DroppedPickupClass, FindDropTransform(), SpawnParameters);

	// The pickup made its containers when it began play, during the spawn.
	EInventoryError Error;
	if (Pickup && TransferItem(Item, Pickup->GetInventory(), 0, false, Error))
	{
		return true;
	}
	if (Pickup)
	{
		Pickup->Destroy();
	}
	return false;
}

FTransform UHadesInventoryComponent::FindDropTransform() const
{
	const AActor* OwnerActor = GetOwner();
	const FVector Start = OwnerActor->GetActorLocation() + OwnerActor->GetActorForwardVector() * DropDistance;
	const FVector End = Start - FVector(0.0, 0.0, 500.0);

	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(HadesDropItem), false, OwnerActor);
	const bool bHitFloor = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	return FTransform(OwnerActor->GetActorRotation(), bHitFloor ? FVector(Hit.ImpactPoint) : Start);
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
	const FInventoryItemHandle Gun = FindGunLoadedBy(Item, Other);
	if (Gun.IsSet())
	{
		if (ReloadGun(Gun))
		{
			return true;
		}
		AHadesPlayerController::ShowMessageTo(GetOwner(), LOCTEXT("GunFull", "It's already fully loaded."));
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
int32 UHadesInventoryComponent::GetLoadedRounds(FInventoryItemHandle Gun) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FHadesGun* GunData = FindGun(Gun);
	if (!InventorySubsystem || !GunData)
	{
		return 0;
	}

	// Kept on the gun once it has been fired or loaded. Until then it has its starting rounds.
	const FGameplayTag Rounds = HadesGameplayTags::Item_Stat_Rounds;
	return InventorySubsystem->HasTagValue(Gun, Rounds)
		? FMath::RoundToInt32(InventorySubsystem->GetTagValue(Gun, Rounds))
		: FMath::Min(GunData->StartingRounds, GunData->Capacity);
}

int32 UHadesInventoryComponent::GetCapacity(FInventoryItemHandle Gun) const
{
	const FHadesGun* GunData = FindGun(Gun);
	return GunData ? GunData->Capacity : 0;
}

int32 UHadesInventoryComponent::GetSpareRounds(FInventoryItemHandle Gun) const
{
	const FHadesGun* GunData = FindGun(Gun);
	return GunData && GunData->Ammo ? CountItems(MakeAmmoQuery(*GunData)) : 0;
}

bool UHadesInventoryComponent::ReloadGun(FInventoryItemHandle Gun)
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FHadesGun* GunData = FindGun(Gun);
	const int32 Loaded = GetLoadedRounds(Gun);
	if (!InventorySubsystem || !GunData || !GunData->Ammo || !OwnsItem(Gun) || Loaded >= GunData->Capacity)
	{
		return false;
	}

	const int32 Taken = RemoveItems(MakeAmmoQuery(*GunData), GunData->Capacity - Loaded);
	return Taken > 0 && InventorySubsystem->SetTagValue(Gun, HadesGameplayTags::Item_Stat_Rounds, Loaded + Taken);
}

bool UHadesInventoryComponent::SpendRounds(FInventoryItemHandle Gun, int32 Count)
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const int32 Loaded = GetLoadedRounds(Gun);
	if (!InventorySubsystem || !FindGun(Gun) || Count <= 0 || Count > Loaded)
	{
		return false;
	}
	return InventorySubsystem->SetTagValue(Gun, HadesGameplayTags::Item_Stat_Rounds, Loaded - Count);
}

const FHadesGun* UHadesInventoryComponent::FindGun(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	return Definition ? Definition->FindFragment<FHadesGun>() : nullptr;
}

FInventoryItemQuery UHadesInventoryComponent::MakeAmmoQuery(const FHadesGun& Gun) const
{
	FInventoryItemQuery Query;
	Query.Definitions.Add(Gun.Ammo);
	return Query;
}

FInventoryItemHandle UHadesInventoryComponent::FindGunLoadedBy(FInventoryItemHandle Item, FInventoryItemHandle Other) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	auto Loads = [&](FInventoryItemHandle Gun, FInventoryItemHandle Ammo)
	{
		const FHadesGun* GunData = FindGun(Gun);
		return InventorySubsystem && GunData && GunData->Ammo && InventorySubsystem->GetItemDefinition(Ammo) == GunData->Ammo;
	};
	return Loads(Item, Other) ? Item : Loads(Other, Item) ? Other : FInventoryItemHandle();
}

bool UHadesInventoryComponent::IsKeyItem(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	return InventorySubsystem && InventorySubsystem->HasItemTag(Item, HadesGameplayTags::Item_Type_KeyItem);
}

// Pockets
bool UHadesInventoryComponent::UpgradePockets()
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FInventoryContainerHandle Pockets = GetContainer(HadesGameplayTags::Container_Pockets);
	const UInventoryContainerLayout* Current = InventorySubsystem ? InventorySubsystem->GetContainerLayout(Pockets) : nullptr;
	if (!Current)
	{
		return false;
	}

	// The starting layout isn't in the list, so it counts as the one before the first upgrade.
	const int32 Next = PocketUpgrades.IndexOfByPredicate([Current](const TObjectPtr<UInventoryContainerLayout>& Layout) { return Layout.Get() == Current; }) + 1;
	EInventoryError Error;
	return PocketUpgrades.IsValidIndex(Next) && InventorySubsystem->SetContainerLayout(Pockets, PocketUpgrades[Next], Error);
}

bool UHadesInventoryComponent::OrganizePockets()
{
	UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	EInventoryError Error;
	return InventorySubsystem && InventorySubsystem->SortContainer(GetContainer(HadesGameplayTags::Container_Pockets), Error);
}

#undef LOCTEXT_NAMESPACE
