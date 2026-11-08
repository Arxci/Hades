#include "HadesInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Algo/AllOf.h"
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

	FText GetItemName(const UInventorySubsystem& InventorySubsystem, FInventoryItemHandle Item)
	{
		const UInventoryItemDefinition* Definition = InventorySubsystem.GetItemDefinition(Item);
		return Definition ? Definition->DisplayName : FText::GetEmpty();
	}
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

	EInventoryError Error;
	if (Equipment->EquipItem(Item, Hand, Error))
	{
		return true;
	}
	if (Error == EInventoryError::NoSpace)
	{
		AHadesPlayerController::ShowMessageTo(GetOwner(), LOCTEXT("NoRoomForHands", "No room to put away what's in your hands."));
	}
	return false;
}

bool UHadesInventoryComponent::StowItem(FInventoryItemHandle Item)
{
	UInventoryEquipmentComponent* Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(GetOwner());
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	if (!Equipment || !InventorySubsystem || !IsInHand(Item))
	{
		return false;
	}

	EInventoryError Error;
	if (Equipment->UnequipItem(Item, Error))
	{
		return true;
	}
	AHadesPlayerController::ShowMessageTo(GetOwner(), FText::Format(LOCTEXT("NoRoomToStow", "No room to put away the {0}."), GetItemName(*InventorySubsystem, Item)));
	return false;
}

bool UHadesInventoryComponent::CanDraw(FInventoryItemHandle Item) const
{
	return OwnsItem(Item) && ChooseHand(Item).IsValid();
}

bool UHadesInventoryComponent::IsInHand(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const FInventoryContainerHandle Hands = GetContainer(HadesGameplayTags::Container_Hands);
	return InventorySubsystem && Hands.IsSet() && InventorySubsystem->GetItemContainer(Item) == Hands;
}

FGameplayTag UHadesInventoryComponent::ChooseHand(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	const UInventoryEquipRules* Rules = Definition ? Definition->EquipRules.Get() : nullptr;
	if (!Rules)
	{
		return FGameplayTag();
	}

	// An empty hand first, so a knife that fits either hand leaves the pistol in the right one.
	const FInventoryContainerHandle Hands = GetContainer(HadesGameplayTags::Container_Hands);
	FGameplayTag Chosen;
	for (const FGameplayTag& Hand : { FGameplayTag(HadesGameplayTags::Equipment_Slot_Hand_Right), FGameplayTag(HadesGameplayTags::Equipment_Slot_Hand_Left) })
	{
		if (!Rules->AllowsSlot(Hand))
		{
			continue;
		}
		if (!InventorySubsystem->GetItemInSlot(Hands, Hand).IsSet())
		{
			return Hand;
		}
		if (!Chosen.IsValid())
		{
			Chosen = Hand;
		}
	}
	return Chosen;
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
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	if (!InventorySubsystem || !CombineRecipes || !OwnsItem(Item))
	{
		return false;
	}

	for (const UInventoryRecipe* Recipe : CombineRecipes->GetRecipes())
	{
		const bool bUsesItem = Recipe->Ingredients.ContainsByPredicate([&](const FInventoryRecipeIngredient& Ingredient)
		{
			return InventorySubsystem->MatchesQuery(Item, Ingredient.Query);
		});
		if (bUsesItem)
		{
			return true;
		}
	}
	return false;
}

bool UHadesInventoryComponent::CombineItems(FInventoryItemHandle Item, FInventoryItemHandle Other)
{
	UInventoryRecipe* Recipe = FindCombineRecipe(Item, Other);
	if (!Recipe)
	{
		AHadesPlayerController::ShowMessageTo(GetOwner(), LOCTEXT("CantCombine", "Those can't be combined."));
		return false;
	}

	TArray<FInventoryItemHandle> Made;
	EInventoryError Error;
	if (Craft(Recipe, FGameplayTagContainer(), 1, Made, Error))
	{
		return true;
	}
	AHadesPlayerController::ShowMessageTo(GetOwner(), Error == EInventoryError::NoSpace
		? LOCTEXT("NoRoomToCombine", "Not enough space.")
		: LOCTEXT("CantCombineNow", "You can't combine those right now."));
	return false;
}

UInventoryRecipe* UHadesInventoryComponent::FindCombineRecipe(FInventoryItemHandle Item, FInventoryItemHandle Other) const
{
	if (!CombineRecipes || Item == Other || !OwnsItem(Item) || !OwnsItem(Other))
	{
		return nullptr;
	}

	for (UInventoryRecipe* Recipe : CombineRecipes->GetRecipes())
	{
		if (IsMadeFrom(*Recipe, Item, Other))
		{
			return Recipe;
		}
	}
	return nullptr;
}

bool UHadesInventoryComponent::IsMadeFrom(const UInventoryRecipe& Recipe, FInventoryItemHandle Item, FInventoryItemHandle Other) const
{
	const UInventorySubsystem* InventorySubsystem = UInventorySubsystem::Get(this);
	if (!InventorySubsystem || Recipe.Ingredients.IsEmpty() || Recipe.Ingredients.Num() > 2)
	{
		return false;
	}

	// Every ingredient comes from one of the two, and both are used: green + green for one ingredient of two, or green + red for two.
	auto Feeds = [InventorySubsystem](FInventoryItemHandle Candidate, const FInventoryRecipeIngredient& Ingredient)
	{
		return InventorySubsystem->MatchesQuery(Candidate, Ingredient.Query);
	};
	auto FeedsAny = [&](FInventoryItemHandle Candidate)
	{
		return Recipe.Ingredients.ContainsByPredicate([&](const FInventoryRecipeIngredient& Ingredient) { return Feeds(Candidate, Ingredient); });
	};
	const bool bAllFed = Algo::AllOf(Recipe.Ingredients, [&](const FInventoryRecipeIngredient& Ingredient)
	{
		return Feeds(Item, Ingredient) || Feeds(Other, Ingredient);
	});
	return bAllFed && FeedsAny(Item) && FeedsAny(Other);
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
