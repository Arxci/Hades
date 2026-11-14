#include "Equipment/InventoryEquipmentComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "AttributeSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"

#include "Data/InventoryContainerLayout.h"
#include "Data/InventoryEquipRules.h"
#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "Equipment/InventoryEquipmentActorInterface.h"
#include "Equipment/InventoryEquipmentPiece.h"
#include "InventoryComponent.h"
#include "InventoryGameplayTags.h"
#include "InventorySubsystem.h"
#include "ModularInventory.h"

namespace
{
	// Handlers that keep changing the inventory in response to equipping would otherwise loop forever.
	constexpr int32 MaxRefreshPasses = 8;

	USceneComponent* FindSceneComponent(const AActor* Actor, FName NameOrTag)
	{
		if (!Actor || NameOrTag.IsNone())
		{
			return nullptr;
		}

		TInlineComponentArray<USceneComponent*> Components(Actor);
		for (USceneComponent* Component : Components)
		{
			if (Component->ComponentHasTag(NameOrTag) || Component->GetFName() == NameOrTag)
			{
				return Component;
			}
		}
		return nullptr;
	}
}

UInventoryEquipmentComponent::UInventoryEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UInventoryEquipmentComponent* UInventoryEquipmentComponent::FindEquipmentComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UInventoryEquipmentComponent>() : nullptr;
}

void UInventoryEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = UInventorySubsystem::Get(this);
	Inventory = UInventoryComponent::FindInventoryComponent(GetOwner());
	if (!Inventory)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Equipment] %s needs an inventory component on %s to hold its slots."), *GetNameSafe(this), *GetNameSafe(GetOwner()));
		return;
	}

	Inventory->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	RefreshEquipment();
}

void UInventoryEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	for (int32 Index = Equipped.Num() - 1; Index >= 0; --Index)
	{
		Unequip(Index);
	}

	Super::EndPlay(EndPlayReason);
}

// Requests
bool UInventoryEquipmentComponent::EquipItem(FInventoryItemHandle Item, FGameplayTag Slot, EInventoryError& OutError)
{
	UInventorySubsystem* InventorySubsystem = Subsystem.Get();
	if (!InventorySubsystem || !Inventory)
	{
		OutError = EInventoryError::InvalidContainer;
		return false;
	}

	FInventoryContainerHandle Target;
	for (const FInventoryContainerHandle& Container : Inventory->GetEquipmentContainers())
	{
		const UInventoryContainerLayout* Layout = InventorySubsystem->GetContainerLayout(Container);
		if (Layout && Layout->FindSlot(Slot) != INDEX_NONE)
		{
			Target = Container;
			break;
		}
	}
	if (!Target.IsSet())
	{
		OutError = EInventoryError::OutOfBounds;
		return false;
	}

	OutError = InventorySubsystem->GetRegistry().EquipItem(Item, Target, Slot, Inventory->GetStorageContainers());
	return OutError == EInventoryError::None;
}

bool UInventoryEquipmentComponent::EquipInPlace(FInventoryItemHandle Item, FGameplayTag Slot, EInventoryError& OutError)
{
	const UInventorySubsystem* InventorySubsystem = Subsystem.Get();
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	if (!Definition || !Inventory || !Inventory->OwnsItem(Item) || InventorySubsystem->IsItemEquipped(Item))
	{
		OutError = EInventoryError::InvalidItem;
		return false;
	}
	const UInventoryEquipRules* Rules = Definition->EquipRules;
	if (!Rules || !Rules->AllowsSlot(Slot) || !Rules->MeetsOwnerRequirements(GetOwner()))
	{
		OutError = EInventoryError::NotEquippable;
		return false;
	}

	// What's in the way stops being equipped, as equipping into a slot makes way: the item in that slot, this item in another,
	// and items the equip rules keep apart from it, like an off-hand item for a two-handed weapon.
	for (auto It = InPlaceItems.CreateIterator(); It; ++It)
	{
		const UInventoryItemDefinition* OtherDefinition = InventorySubsystem->GetItemDefinition(It.Value());
		const UInventoryEquipRules* OtherRules = OtherDefinition ? OtherDefinition->EquipRules.Get() : nullptr;
		const bool bInTheWay = It.Key() == Slot || It.Value() == Item || Rules->BlocksSlot(It.Key()) || (OtherRules && OtherRules->BlocksSlot(Slot));
		if (bInTheWay)
		{
			It.RemoveCurrent();
		}
	}
	InPlaceItems.Add(Slot, Item);
	RefreshEquipment();
	OutError = EInventoryError::None;
	return true;
}

bool UInventoryEquipmentComponent::UnequipItem(FInventoryItemHandle Item, EInventoryError& OutError)
{
	if (const FGameplayTag* Found = InPlaceItems.FindKey(Item))
	{
		const FGameplayTag InPlaceSlot = *Found;
		InPlaceItems.Remove(InPlaceSlot);
		RefreshEquipment();
		OutError = EInventoryError::None;
		return true;
	}

	UInventorySubsystem* InventorySubsystem = Subsystem.Get();
	if (!InventorySubsystem || !Inventory || !InventorySubsystem->IsItemEquipped(Item))
	{
		OutError = EInventoryError::InvalidItem;
		return false;
	}

	OutError = InventorySubsystem->GetRegistry().MoveItemToContainers(Item, Inventory->GetStorageContainers());
	return OutError == EInventoryError::None;
}

bool UInventoryEquipmentComponent::UnequipSlot(FGameplayTag Slot, EInventoryError& OutError)
{
	return UnequipItem(GetEquippedItem(Slot), OutError);
}

// Queries
FInventoryItemHandle UInventoryEquipmentComponent::GetEquippedItem(FGameplayTag Slot) const
{
	const FInventoryEquippedItem* Entry = Equipped.FindByPredicate([&Slot](const FInventoryEquippedItem& Candidate) { return Candidate.Slot == Slot; });
	return Entry ? Entry->Item : FInventoryItemHandle();
}

TArray<FInventoryItemHandle> UInventoryEquipmentComponent::GetEquippedItems() const
{
	TArray<FInventoryItemHandle> Items;
	for (const FInventoryEquippedItem& Entry : Equipped)
	{
		Items.Add(Entry.Item);
	}
	return Items;
}

TArray<UObject*> UInventoryEquipmentComponent::GetSpawnedPieces(FInventoryItemHandle Item) const
{
	TArray<UObject*> Spawned;
	if (const FInventoryEquippedItem* Entry = FindEntry(Item))
	{
		for (const FInventorySpawnedPiece& Piece : Entry->Pieces)
		{
			Spawned.Add(Piece.Spawned);
		}
	}
	return Spawned;
}

FInventoryItemHandle UInventoryEquipmentComponent::FindEquippedItemFor(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = Subsystem.Get();
	const UInventoryItemDefinition* Definition = InventorySubsystem ? InventorySubsystem->GetItemDefinition(Item) : nullptr;
	const UInventoryEquipRules* Rules = Definition ? Definition->EquipRules.Get() : nullptr;
	if (!Rules || IsEquipped(Item))
	{
		return FInventoryItemHandle();
	}

	const FInventoryEquippedItem* Entry = Equipped.FindByPredicate([Rules](const FInventoryEquippedItem& Candidate) { return Rules->AllowsSlot(Candidate.Slot); });
	return Entry ? Entry->Item : FInventoryItemHandle();
}

FInventoryItemHandle UInventoryEquipmentComponent::GetItemForAbility(FGameplayAbilitySpecHandle Ability) const
{
	const FInventoryEquippedItem* Entry = Equipped.FindByPredicate([&Ability](const FInventoryEquippedItem& Candidate) { return Candidate.Abilities.Contains(Ability); });
	return Entry ? Entry->Item : FInventoryItemHandle();
}

const FInventoryEquippedItem* UInventoryEquipmentComponent::FindEntry(FInventoryItemHandle Item) const
{
	return Item.IsSet() ? Equipped.FindByPredicate([&Item](const FInventoryEquippedItem& Entry) { return Entry.Item == Item; }) : nullptr;
}

FInventoryEquippedItem* UInventoryEquipmentComponent::FindEntry(FInventoryItemHandle Item)
{
	return Item.IsSet() ? Equipped.FindByPredicate([&Item](const FInventoryEquippedItem& Entry) { return Entry.Item == Item; }) : nullptr;
}

UAbilitySystemComponent* UInventoryEquipmentComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

int32 UInventoryEquipmentComponent::GetItemLevel(FInventoryItemHandle Item) const
{
	const UInventorySubsystem* InventorySubsystem = Subsystem.Get();
	const float Level = InventorySubsystem ? InventorySubsystem->GetTagValue(Item, InventoryGameplayTags::Item_Level) : 0.0f;
	return FMath::Max(1, FMath::RoundToInt(Level));
}

// Reconciling with the slots
void UInventoryEquipmentComponent::HandleInventoryChanged(UInventoryComponent* ChangedInventory)
{
	RefreshEquipment();
}

void UInventoryEquipmentComponent::RefreshEquipment()
{
	// Equipping runs game code that may change the inventory again; that only schedules another pass.
	if (bRefreshing)
	{
		bRefreshPending = true;
		return;
	}

	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	int32 Passes = 0;
	do
	{
		bRefreshPending = false;
		Reconcile();
	}
	while (bRefreshPending && ++Passes < MaxRefreshPasses);

	if (bRefreshPending)
	{
		UE_LOG(LogInventory, Warning, TEXT("[Equipment] %s kept changing while equipping; stopped after %d passes."), *GetNameSafe(GetOwner()), MaxRefreshPasses);
	}
}

void UInventoryEquipmentComponent::Reconcile()
{
	if (!Subsystem.IsValid() || !Inventory)
	{
		return;
	}

	TArray<FWantedItem> Wanted;
	for (const FInventoryContainerHandle& Container : Inventory->GetEquipmentContainers())
	{
		GatherWantedItems(Container, FInventoryItemHandle(), Wanted);
	}

	const FInventoryRegistry& Registry = Subsystem->GetRegistry();
	for (auto It = InPlaceItems.CreateIterator(); It; ++It)
	{
		const FInventoryItemRecord* ItemRecord = Inventory->OwnsItem(It.Value()) ? Registry.FindItem(It.Value()) : nullptr;
		if (!ItemRecord)
		{
			It.RemoveCurrent();
			continue;
		}

		Wanted.Add({ It.Value(), It.Key(), FInventoryItemHandle() });
		for (const FInventoryContainerHandle& Nested : ItemRecord->Containers)
		{
			GatherWantedItems(Nested, It.Value(), Wanted);
		}
	}

	// Entries are in parent-first order, so a removed parent is known before its attachments are checked.
	// Attachments go with it and come back below, re-attaching to the parent's new pieces.
	TSet<FInventoryItemHandle> Removed;
	for (const FInventoryEquippedItem& Entry : Equipped)
	{
		const bool bStillWanted = Wanted.ContainsByPredicate([&Entry](const FWantedItem& Want)
		{
			return Want.Item == Entry.Item && Want.Slot == Entry.Slot && Want.ParentItem == Entry.ParentItem;
		});
		if (!bStillWanted || Removed.Contains(Entry.ParentItem))
		{
			Removed.Add(Entry.Item);
		}
	}

	TArray<FWantedItem> Unequipped;
	for (int32 Index = Equipped.Num() - 1; Index >= 0; --Index)
	{
		if (Removed.Contains(Equipped[Index].Item))
		{
			Unequipped.Add({ Equipped[Index].Item, Equipped[Index].Slot, Equipped[Index].ParentItem });
			Unequip(Index);
		}
	}
	if (!Unequipped.IsEmpty())
	{
		RelinkAnimLayers();
	}

	TArray<FWantedItem> NewlyEquipped;
	for (const FWantedItem& Want : Wanted)
	{
		if (!FindEntry(Want.Item))
		{
			Equip(Want);
			NewlyEquipped.Add(Want);
		}
	}

	for (FInventoryEquippedItem& Entry : Equipped)
	{
		const int32 Level = GetItemLevel(Entry.Item);
		if (Level != Entry.Level)
		{
			SetLevel(Entry, Level);
		}
	}

	ApplyLoadedCosmetics();

	for (const FWantedItem& Want : Unequipped)
	{
		OnItemUnequipped.Broadcast(Want.Item, Want.Slot);
	}
	for (const FWantedItem& Want : NewlyEquipped)
	{
		OnItemEquipped.Broadcast(Want.Item, Want.Slot);
	}
}

void UInventoryEquipmentComponent::GatherWantedItems(FInventoryContainerHandle Container, FInventoryItemHandle ParentItem, TArray<FWantedItem>& OutWanted) const
{
	const FInventoryRegistry& Registry = Subsystem->GetRegistry();
	const FInventoryContainerRecord* ContainerRecord = Registry.FindContainer(Container);
	if (!ContainerRecord || !ContainerRecord->Layout->bEquipment)
	{
		return;
	}

	for (const FInventoryItemHandle& Item : ContainerRecord->Items)
	{
		const FInventoryItemRecord* ItemRecord = Registry.FindItem(Item);
		if (!ItemRecord || !ContainerRecord->Layout->Slots.IsValidIndex(ItemRecord->Placement.Index))
		{
			continue;
		}

		OutWanted.Add({ Item, ContainerRecord->Layout->Slots[ItemRecord->Placement.Index].Tag, ParentItem });
		for (const FInventoryContainerHandle& Nested : ItemRecord->Containers)
		{
			GatherWantedItems(Nested, Item, OutWanted);
		}
	}
}

void UInventoryEquipmentComponent::Equip(const FWantedItem& Want)
{
	const UInventoryItemDefinition* Definition = Subsystem->GetItemDefinition(Want.Item);
	if (!Definition)
	{
		return;
	}

	FInventoryEquippedItem& Entry = Equipped.AddDefaulted_GetRef();
	Entry.Item = Want.Item;
	Entry.Slot = Want.Slot;
	Entry.ParentItem = Want.ParentItem;
	Entry.Level = GetItemLevel(Want.Item);

	if (Definition->EquipRules)
	{
		GrantAbilitySystem(Entry, *Definition->EquipRules);
	}

	TArray<FSoftObjectPath> Assets;
	if (Definition->Cosmetics)
	{
		Definition->Cosmetics->GatherEquipAssets(Assets);
	}
	if (Assets.IsEmpty())
	{
		Entry.bCosmeticsLoaded = true;
		return;
	}

	// When everything is already loaded the callback can run before this returns, so it looks the entry up by item.
	const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Assets,
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleCosmeticsLoaded, Want.Item));
	if (FInventoryEquippedItem* Loading = FindEntry(Want.Item))
	{
		Loading->CosmeticsHandle = Handle;
	}
}

void UInventoryEquipmentComponent::Unequip(int32 EntryIndex)
{
	FInventoryEquippedItem& Entry = Equipped[EntryIndex];
	RemoveCosmetics(Entry);
	RemoveAbilitySystem(Entry);
	Equipped.RemoveAt(EntryIndex);
}

// Ability system
void UInventoryEquipmentComponent::GrantAbilitySystem(FInventoryEquippedItem& Entry, const UInventoryEquipRules& Rules)
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (!AbilitySystem)
	{
		return;
	}

	for (const FInventoryAbilityGrant& Grant : Rules.Abilities)
	{
		FGameplayAbilitySpecHandle Handle;
		if (Grant.Ability)
		{
			// The source object lets an ability find its item again through GetItemForAbility.
			Handle = AbilitySystem->GiveAbility(FGameplayAbilitySpec(Grant.Ability, Grant.GetLevel(Entry.Level), INDEX_NONE, this));
		}
		Entry.Abilities.Add(Handle);
	}

	for (const FInventoryEffectGrant& Grant : Rules.Effects)
	{
		FActiveGameplayEffectHandle Handle;
		if (Grant.Effect)
		{
			FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
			Context.AddSourceObject(this);
			const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Grant.Effect, Grant.GetLevel(Entry.Level), Context);
			if (Spec.IsValid())
			{
				Handle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
		Entry.Effects.Add(Handle);
	}

	for (const TSubclassOf<UAttributeSet>& SetClass : Rules.AttributeSets)
	{
		if (SetClass)
		{
			UAttributeSet* Set = NewObject<UAttributeSet>(AbilitySystem->GetOwner(), SetClass);
			AbilitySystem->AddAttributeSetSubobject(Set);
			Entry.AttributeSets.Add(Set);
		}
	}

	if (!Rules.OwnerTags.IsEmpty())
	{
		AbilitySystem->AddLooseGameplayTags(Rules.OwnerTags);
		Entry.OwnerTags = Rules.OwnerTags;
	}
}

void UInventoryEquipmentComponent::RemoveAbilitySystem(FInventoryEquippedItem& Entry)
{
	if (UAbilitySystemComponent* AbilitySystem = GetAbilitySystem())
	{
		for (const FGameplayAbilitySpecHandle& Handle : Entry.Abilities)
		{
			if (Handle.IsValid())
			{
				AbilitySystem->ClearAbility(Handle);
			}
		}
		for (const FActiveGameplayEffectHandle& Handle : Entry.Effects)
		{
			if (Handle.IsValid())
			{
				AbilitySystem->RemoveActiveGameplayEffect(Handle);
			}
		}
		for (UAttributeSet* Set : Entry.AttributeSets)
		{
			if (Set)
			{
				AbilitySystem->RemoveSpawnedAttribute(Set);
			}
		}
		if (!Entry.OwnerTags.IsEmpty())
		{
			AbilitySystem->RemoveLooseGameplayTags(Entry.OwnerTags);
		}
	}

	Entry.Abilities.Reset();
	Entry.Effects.Reset();
	Entry.AttributeSets.Reset();
	Entry.OwnerTags.Reset();
}

void UInventoryEquipmentComponent::SetLevel(FInventoryEquippedItem& Entry, int32 NewLevel)
{
	Entry.Level = NewLevel;

	const UInventoryItemDefinition* Definition = Subsystem->GetItemDefinition(Entry.Item);
	const UInventoryEquipRules* Rules = Definition ? Definition->EquipRules.Get() : nullptr;
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystem();
	if (!Rules || !AbilitySystem)
	{
		return;
	}

	for (int32 Index = 0; Index < Entry.Abilities.Num() && Index < Rules->Abilities.Num(); ++Index)
	{
		if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Entry.Abilities[Index]))
		{
			Spec->Level = Rules->Abilities[Index].GetLevel(NewLevel);
			AbilitySystem->MarkAbilitySpecDirty(*Spec);
		}
	}

	for (int32 Index = 0; Index < Entry.Effects.Num() && Index < Rules->Effects.Num(); ++Index)
	{
		if (Entry.Effects[Index].IsValid())
		{
			AbilitySystem->SetActiveGameplayEffectLevel(Entry.Effects[Index], FMath::RoundToInt(Rules->Effects[Index].GetLevel(NewLevel)));
		}
	}
}

// Cosmetics
void UInventoryEquipmentComponent::HandleCosmeticsLoaded(FInventoryItemHandle Item)
{
	if (FInventoryEquippedItem* Entry = FindEntry(Item))
	{
		Entry->bCosmeticsLoaded = true;
		RefreshEquipment();
	}
}

void UInventoryEquipmentComponent::ApplyLoadedCosmetics()
{
	for (int32 Index = 0; Index < Equipped.Num(); ++Index)
	{
		FInventoryEquippedItem& Entry = Equipped[Index];
		const FInventoryEquippedItem* Parent = FindEntry(Entry.ParentItem);
		if (Entry.bCosmeticsApplied || !Entry.bCosmeticsLoaded || (Parent && !Parent->bCosmeticsApplied))
		{
			continue;
		}

		const UInventoryItemDefinition* Definition = Subsystem->GetItemDefinition(Entry.Item);
		if (Definition && Definition->Cosmetics)
		{
			ApplyCosmetics(Entry, *Definition->Cosmetics);
		}
		Entry.bCosmeticsApplied = true;
	}
}

void UInventoryEquipmentComponent::ApplyCosmetics(FInventoryEquippedItem& Entry, const UInventoryItemCosmetics& Cosmetics)
{
	for (const UInventoryEquipmentPiece* Piece : Cosmetics.Pieces)
	{
		UObject* Spawned = Piece ? Piece->Spawn(GetOwner(), FindAttachParent(*Piece, Entry)) : nullptr;
		if (!Spawned)
		{
			continue;
		}

		FInventorySpawnedPiece& SpawnedPiece = Entry.Pieces.AddDefaulted_GetRef();
		SpawnedPiece.Piece = Piece;
		SpawnedPiece.Spawned = Spawned;

		if (Spawned->Implements<UInventoryEquipmentActorInterface>())
		{
			IInventoryEquipmentActorInterface::Execute_OnEquipped(Spawned, this, Entry.Item, Entry.Slot);
		}
	}

	for (const FInventoryAnimLayer& AnimLayer : Cosmetics.AnimLayers)
	{
		USkeletalMeshComponent* Mesh = FindSkeletalMesh(AnimLayer.TargetMesh);
		UClass* LayerClass = AnimLayer.LayerClass.Get();
		if (Mesh && LayerClass)
		{
			Mesh->LinkAnimClassLayers(LayerClass);
			FInventoryLinkedAnimLayer& Linked = Entry.AnimLayers.AddDefaulted_GetRef();
			Linked.Mesh = Mesh;
			Linked.Layer = LayerClass;
		}
	}
}

void UInventoryEquipmentComponent::RemoveCosmetics(FInventoryEquippedItem& Entry)
{
	if (Entry.CosmeticsHandle.IsValid())
	{
		if (Entry.CosmeticsHandle->IsLoadingInProgress())
		{
			Entry.CosmeticsHandle->CancelHandle();
		}
		Entry.CosmeticsHandle.Reset();
	}

	for (const FInventorySpawnedPiece& Piece : Entry.Pieces)
	{
		UObject* Spawned = Piece.Spawned;
		if (!IsValid(Spawned))
		{
			continue;
		}

		if (Spawned->Implements<UInventoryEquipmentActorInterface>())
		{
			IInventoryEquipmentActorInterface::Execute_OnUnequipped(Spawned, this, Entry.Item, Entry.Slot);
		}
		if (Piece.Piece)
		{
			Piece.Piece->Despawn(Spawned);
		}
	}
	Entry.Pieces.Reset();

	for (const FInventoryLinkedAnimLayer& Linked : Entry.AnimLayers)
	{
		if (USkeletalMeshComponent* Mesh = Linked.Mesh.Get())
		{
			Mesh->UnlinkAnimClassLayers(Linked.Layer);
		}
	}
	Entry.AnimLayers.Reset();
	Entry.bCosmeticsApplied = false;
}

void UInventoryEquipmentComponent::RelinkAnimLayers()
{
	for (const FInventoryEquippedItem& Entry : Equipped)
	{
		for (const FInventoryLinkedAnimLayer& Linked : Entry.AnimLayers)
		{
			if (USkeletalMeshComponent* Mesh = Linked.Mesh.Get())
			{
				Mesh->LinkAnimClassLayers(Linked.Layer);
			}
		}
	}
}

USceneComponent* UInventoryEquipmentComponent::FindAttachParent(const UInventoryEquipmentPiece& Piece, const FInventoryEquippedItem& Entry) const
{
	if (const FInventoryEquippedItem* Parent = FindEntry(Entry.ParentItem))
	{
		for (const FInventorySpawnedPiece& ParentPiece : Parent->Pieces)
		{
			if (const AActor* ParentActor = Cast<AActor>(ParentPiece.Spawned))
			{
				USceneComponent* Named = FindSceneComponent(ParentActor, Piece.AttachComponent);
				return Named ? Named : ParentActor->GetRootComponent();
			}
			if (USceneComponent* ParentComponent = Cast<USceneComponent>(ParentPiece.Spawned))
			{
				return ParentComponent;
			}
		}
	}

	const AActor* Owner = GetOwner();
	if (USceneComponent* Named = FindSceneComponent(Owner, Piece.AttachComponent))
	{
		return Named;
	}
	if (!Piece.AttachComponent.IsNone())
	{
		UE_LOG(LogInventory, Warning, TEXT("[Equipment] %s has no component named or tagged %s; attaching to the default."), *GetNameSafe(Owner), *Piece.AttachComponent.ToString());
	}

	const ACharacter* Character = Cast<ACharacter>(Owner);
	return Character ? Character->GetMesh() : Owner->GetRootComponent();
}

USkeletalMeshComponent* UInventoryEquipmentComponent::FindSkeletalMesh(FName NameOrTag) const
{
	if (NameOrTag.IsNone())
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		return Character ? Character->GetMesh() : nullptr;
	}
	return Cast<USkeletalMeshComponent>(FindSceneComponent(GetOwner(), NameOrTag));
}
