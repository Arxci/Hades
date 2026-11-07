#include "InventorySubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/ActorComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Crafting/InventoryRecipe.h"
#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemDefinition.h"

namespace
{
	bool Succeeded(EInventoryError Error, EInventoryError& OutError)
	{
		OutError = Error;
		return Error == EInventoryError::None;
	}
}

UInventorySubsystem* UInventorySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UInventorySubsystem>() : nullptr;
}

void UInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegistryChangedHandle = Registry.OnChanged.AddUObject(this, &ThisClass::HandleRegistryChanged);
	Registry.CanEnterHook = [this](const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container)
	{
		return CheckOwnerRequirements(InRegistry, Item, Container);
	};
}

void UInventorySubsystem::Deinitialize()
{
	Registry.OnChanged.Remove(RegistryChangedHandle);
	Registry.CanEnterHook = nullptr;

	Super::Deinitialize();
}

bool UInventorySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UInventorySubsystem::HandleRegistryChanged(const FInventoryChangeSet& Changes)
{
	for (const FInventoryContainerHandle& Container : Changes.Containers)
	{
		OnContainerChanged.Broadcast(Container);
	}
	for (const FInventoryItemHandle& Item : Changes.Items)
	{
		OnItemChanged.Broadcast(Item);
	}
}

EInventoryError UInventorySubsystem::CheckOwnerRequirements(const FInventoryRegistry& InRegistry, const FInventoryItemRecord& Item, FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* ContainerRecord = InRegistry.FindContainer(Container);
	const UInventoryEquipRules* Rules = Item.Definition ? Item.Definition->EquipRules.Get() : nullptr;
	if (!ContainerRecord || !ContainerRecord->Layout->bEquipment || !Rules || Rules->OwnerRequirements.IsEmpty())
	{
		return EInventoryError::None;
	}

	UObject* Owner = InRegistry.GetOwner(Container);
	const UActorComponent* OwnerComponent = Cast<UActorComponent>(Owner);
	const AActor* OwnerActor = OwnerComponent ? OwnerComponent->GetOwner() : Cast<AActor>(Owner);

	// Without an ability system there is nothing to check, which keeps looting corpses and chests simple.
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!AbilitySystem)
	{
		return EInventoryError::None;
	}

	FGameplayTagContainer OwnerTags;
	AbilitySystem->GetOwnedGameplayTags(OwnerTags);
	return Rules->OwnerRequirements.RequirementsMet(OwnerTags) ? EInventoryError::None : EInventoryError::NotEquippable;
}

// Items
const UInventoryItemDefinition* UInventorySubsystem::GetItemDefinition(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	return Record ? Record->Definition.Get() : nullptr;
}

int32 UInventorySubsystem::GetItemCount(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	return Record ? Record->Count : 0;
}

FInventoryContainerHandle UInventorySubsystem::GetItemContainer(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	return Record ? Record->Container : FInventoryContainerHandle();
}

TArray<FInventoryContainerHandle> UInventorySubsystem::GetItemContainers(FInventoryItemHandle Item) const
{
	const FInventoryItemRecord* Record = Registry.FindItem(Item);
	return Record ? Record->Containers : TArray<FInventoryContainerHandle>();
}

bool UInventorySubsystem::IsItemEquipped(FInventoryItemHandle Item) const
{
	const FInventoryContainerRecord* Container = Registry.FindContainer(GetItemContainer(Item));
	return Container && Container->Layout->bEquipment;
}

// Tags, values and fragments
bool UInventorySubsystem::SetTagValue(FInventoryItemHandle Item, FGameplayTag Tag, float Value)
{
	return Registry.SetTagValue(Item, Tag, Value) == EInventoryError::None;
}

bool UInventorySubsystem::AddToTagValue(FInventoryItemHandle Item, FGameplayTag Tag, float Delta)
{
	return Registry.AddToTagValue(Item, Tag, Delta) == EInventoryError::None;
}

bool UInventorySubsystem::RemoveTagValue(FInventoryItemHandle Item, FGameplayTag Tag)
{
	return Registry.RemoveTagValue(Item, Tag) == EInventoryError::None;
}

bool UInventorySubsystem::AddItemTag(FInventoryItemHandle Item, FGameplayTag Tag)
{
	return Registry.AddItemTag(Item, Tag) == EInventoryError::None;
}

bool UInventorySubsystem::RemoveItemTag(FInventoryItemHandle Item, FGameplayTag Tag)
{
	return Registry.RemoveItemTag(Item, Tag) == EInventoryError::None;
}

bool UInventorySubsystem::RemoveItemFragment(FInventoryItemHandle Item, const UScriptStruct* FragmentType)
{
	return Registry.RemoveItemFragment(Item, FragmentType) == EInventoryError::None;
}

bool UInventorySubsystem::RemoveContainerFragment(FInventoryContainerHandle Container, const UScriptStruct* FragmentType)
{
	return Registry.RemoveContainerFragment(Container, FragmentType) == EInventoryError::None;
}

// Containers
const UInventoryContainerLayout* UInventorySubsystem::GetContainerLayout(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
	return Record ? Record->Layout.Get() : nullptr;
}

FGameplayTag UInventorySubsystem::GetContainerTag(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
	return Record ? Record->Tag : FGameplayTag();
}

TArray<FInventoryItemHandle> UInventorySubsystem::GetContainerItems(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
	return Record ? Record->Items : TArray<FInventoryItemHandle>();
}

FInventoryItemHandle UInventorySubsystem::GetContainerOwnerItem(FInventoryContainerHandle Container) const
{
	const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
	return Record ? Record->OwnerItem : FInventoryItemHandle();
}

FInventoryItemHandle UInventorySubsystem::GetItemInSlot(FInventoryContainerHandle Container, FGameplayTag Slot) const
{
	const FInventoryContainerRecord* Record = Registry.FindContainer(Container);
	const int32 SlotIndex = Record ? Record->Layout->FindSlot(Slot) : INDEX_NONE;
	return SlotIndex != INDEX_NONE ? Registry.GetItemAt(Container, FInventoryPlacement::AtIndex(SlotIndex)) : FInventoryItemHandle();
}

bool UInventorySubsystem::CanPlaceItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryError& OutError) const
{
	return Succeeded(Registry.CanPlace(Item, Container, Placement), OutError);
}

TArray<FInventoryItemHandle> UInventorySubsystem::FindItems(const TArray<FInventoryContainerHandle>& Containers, const FInventoryItemQuery& Query, bool bRecursive) const
{
	TArray<FInventoryItemHandle> Found;
	Registry.FindItems(Containers, Query, bRecursive, Found);
	return Found;
}

FInventoryContainerHandle UInventorySubsystem::CreateContainer(const UInventoryContainerLayout* Layout, FGameplayTag Tag, UObject* Owner)
{
	return Registry.CreateContainer(Layout, Tag, Owner);
}

bool UInventorySubsystem::DestroyContainer(FInventoryContainerHandle Container)
{
	return Registry.DestroyContainer(Container) == EInventoryError::None;
}

// Operations
int32 UInventorySubsystem::AddItem(const FInventoryItemSpec& Spec, FInventoryContainerHandle Container, bool bAllowPartial, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError)
{
	OutItems.Reset();
	return Registry.AddItemToContainers(Spec, MakeArrayView(&Container, 1), bAllowPartial, &OutItems, &OutError);
}

FInventoryItemHandle UInventorySubsystem::AddItemAt(const FInventoryItemSpec& Spec, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryError& OutError)
{
	TArray<FInventoryItemHandle> Added;
	OutError = Registry.AddItem(Spec, Container, Placement, &Added);
	return OutError == EInventoryError::None && !Added.IsEmpty() ? Added[0] : FInventoryItemHandle();
}

bool UInventorySubsystem::MoveItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, int32 Count, EInventoryError& OutError)
{
	return Succeeded(Registry.MoveItem(Item, Container, {}, Count), OutError);
}

bool UInventorySubsystem::MoveItemTo(FInventoryItemHandle Item, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, int32 Count, EInventoryError& OutError)
{
	return Succeeded(Registry.MoveItem(Item, Container, Placement, Count), OutError);
}

bool UInventorySubsystem::MoveItemToContainers(FInventoryItemHandle Item, const TArray<FInventoryContainerHandle>& Containers, int32 Count, bool bAllowPartial, EInventoryError& OutError)
{
	return Succeeded(Registry.MoveItemToContainers(Item, Containers, Count, bAllowPartial), OutError);
}

bool UInventorySubsystem::SwapItems(FInventoryItemHandle Item, FInventoryItemHandle Other, EInventoryError& OutError)
{
	return Succeeded(Registry.SwapItems(Item, Other), OutError);
}

FInventoryItemHandle UInventorySubsystem::SplitStack(FInventoryItemHandle Item, int32 Count, FInventoryContainerHandle Container, EInventoryError& OutError)
{
	FInventoryItemHandle NewItem;
	OutError = Registry.SplitStack(Item, Count, Container, {}, &NewItem);
	return NewItem;
}

FInventoryItemHandle UInventorySubsystem::SplitStackTo(FInventoryItemHandle Item, int32 Count, FInventoryContainerHandle Container, const FInventoryPlacement& Placement, EInventoryError& OutError)
{
	FInventoryItemHandle NewItem;
	OutError = Registry.SplitStack(Item, Count, Container, Placement, &NewItem);
	return NewItem;
}

int32 UInventorySubsystem::MergeStacks(FInventoryItemHandle Source, FInventoryItemHandle Target, int32 Count, EInventoryError& OutError)
{
	int32 Moved = 0;
	OutError = Registry.MergeStacks(Source, Target, Count, &Moved);
	return Moved;
}

bool UInventorySubsystem::RotateItem(FInventoryItemHandle Item, EInventoryError& OutError)
{
	return Succeeded(Registry.RotateItem(Item), OutError);
}

bool UInventorySubsystem::EquipItem(FInventoryItemHandle Item, FInventoryContainerHandle EquipmentContainer, FGameplayTag Slot, EInventoryError& OutError)
{
	return Succeeded(Registry.EquipItem(Item, EquipmentContainer, Slot), OutError);
}

bool UInventorySubsystem::UnequipItem(FInventoryItemHandle Item, FInventoryContainerHandle Container, EInventoryError& OutError)
{
	return Succeeded(Registry.UnequipItem(Item, Container), OutError);
}

// Crafting
bool UInventorySubsystem::CanCraft(const UInventoryRecipe* Recipe, const TArray<FInventoryContainerHandle>& Sources, const FGameplayTagContainer& ContextTags, int32 Times, EInventoryError& OutError) const
{
	if (!Recipe)
	{
		return Succeeded(EInventoryError::InvalidDefinition, OutError);
	}
	return Succeeded(Registry.CanCraft(*Recipe, Sources, ContextTags, Times), OutError);
}

bool UInventorySubsystem::Craft(const UInventoryRecipe* Recipe, const TArray<FInventoryContainerHandle>& Sources, const TArray<FInventoryContainerHandle>& Outputs,
	const FGameplayTagContainer& ContextTags, int32 Times, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError)
{
	OutItems.Reset();
	if (!Recipe)
	{
		return Succeeded(EInventoryError::InvalidDefinition, OutError);
	}

	FRandomStream Stream(FMath::Rand());
	return Succeeded(Registry.Craft(*Recipe, Sources, Outputs, ContextTags, Times, Stream, &OutItems), OutError);
}
