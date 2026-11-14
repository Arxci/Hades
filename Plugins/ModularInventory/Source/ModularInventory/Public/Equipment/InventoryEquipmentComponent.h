#pragma once

#include "CoreMinimal.h"

#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"

#include "InventoryTypes.h"

#include "InventoryEquipmentComponent.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UAttributeSet;
class UInventoryComponent;
class UInventoryEquipmentPiece;
class UInventoryItemCosmetics;
class UInventoryEquipRules;
class UInventorySubsystem;
class USceneComponent;
class USkeletalMeshComponent;


USTRUCT()
struct FInventorySpawnedPiece
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UInventoryEquipmentPiece> Piece;

	UPROPERTY()
	TObjectPtr<UObject> Spawned;
};


USTRUCT()
struct FInventoryLinkedAnimLayer
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY()
	TSubclassOf<UAnimInstance> Layer;
};


/** What equipping one item applied, so it can be taken back. */
USTRUCT()
struct FInventoryEquippedItem
{
	GENERATED_BODY()

	FInventoryItemHandle Item;
	FGameplayTag Slot;

	/** The equipped item whose attachment container holds this one. */
	FInventoryItemHandle ParentItem;

	int32 Level = 1;

	/** Parallel to the equip rules' ability and effect lists, so levels can be updated in place. */
	TArray<FGameplayAbilitySpecHandle> Abilities;
	TArray<FActiveGameplayEffectHandle> Effects;

	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> AttributeSets;

	FGameplayTagContainer OwnerTags;

	UPROPERTY()
	TArray<FInventorySpawnedPiece> Pieces;

	UPROPERTY()
	TArray<FInventoryLinkedAnimLayer> AnimLayers;

	TSharedPtr<FStreamableHandle> CosmeticsHandle;
	bool bCosmeticsLoaded = false;
	bool bCosmeticsApplied = false;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryEquipmentSignature, FInventoryItemHandle, Item, FGameplayTag, Slot);


/**
 * Applies whatever sits in the owner's equipment slots: ability system grants scaled by item level, spawned pieces and linked anim layers.
 * Items in an equipped item's equipment containers, such as weapon attachments, are applied too and their pieces attach to the parent's.
 * Needs an inventory component on the same actor; the equipment slots are its containers with equipment layouts.
 * Items can also be equipped in place, staying wherever they are in the inventory, as the RE games do with the weapon in hand.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class MODULARINVENTORY_API UInventoryEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryEquipmentComponent();

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment", meta = (DefaultToSelf = "Actor"))
	static UInventoryEquipmentComponent* FindEquipmentComponent(const AActor* Actor);

	/**
	 * Moves the item into the first equipment container with that slot. Whatever is in the way, the item in the slot or one its equip rules
	 * keep apart from it, takes the spot the item left, or goes to storage. Fails when there is no room for it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool EquipItem(FInventoryItemHandle Item, FGameplayTag Slot, EInventoryError& OutError);

	/**
	 * Equips an item the owner carries without moving it. Its equip rules must allow the slot and the owner meet their requirements;
	 * use a slot no equipment container has. What's in the way stops being equipped: whatever was equipped in place in that slot, and
	 * items equipped in place that the equip rules keep apart from it. The item stays equipped until unequipped or until it leaves the inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool EquipInPlace(FInventoryItemHandle Item, FGameplayTag Slot, EInventoryError& OutError);

	/** Moves the item from its slot to a free spot in the owner's storage. An item equipped in place just stops being equipped. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool UnequipItem(FInventoryItemHandle Item, EInventoryError& OutError);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool UnequipSlot(FGameplayTag Slot, EInventoryError& OutError);

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FInventoryItemHandle GetEquippedItem(FGameplayTag Slot) const;

	/** Attachments of equipped items included, parents first. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	TArray<FInventoryItemHandle> GetEquippedItems() const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	bool IsEquipped(FInventoryItemHandle Item) const { return FindEntry(Item) != nullptr; }

	/** The item equipped in a slot Item's equip rules allow, such as the weapon it would replace, to compare them. Unset while Item is equipped itself. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FInventoryItemHandle FindEquippedItemFor(FInventoryItemHandle Item) const;

	/** The actors and components the item's pieces spawned. Empty until its cosmetics finish loading. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	TArray<UObject*> GetSpawnedPieces(FInventoryItemHandle Item) const;

	/** The equipped item that granted an ability. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FInventoryItemHandle GetItemForAbility(FGameplayAbilitySpecHandle Ability) const;

	const TArray<FInventoryEquippedItem>& GetEquippedEntries() const { return Equipped; }

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Equipment")
	FInventoryEquipmentSignature OnItemEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Equipment")
	FInventoryEquipmentSignature OnItemUnequipped;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	struct FWantedItem
	{
		FInventoryItemHandle Item;
		FGameplayTag Slot;
		FInventoryItemHandle ParentItem;
	};

	UFUNCTION()
	void HandleInventoryChanged(UInventoryComponent* ChangedInventory);

	void RefreshEquipment();
	void Reconcile();
	void GatherWantedItems(FInventoryContainerHandle Container, FInventoryItemHandle ParentItem, TArray<FWantedItem>& OutWanted) const;

	void Equip(const FWantedItem& Wanted);
	void Unequip(int32 EntryIndex);
	void SetLevel(FInventoryEquippedItem& Entry, int32 NewLevel);
	int32 GetItemLevel(FInventoryItemHandle Item) const;

	void GrantAbilitySystem(FInventoryEquippedItem& Entry, const UInventoryEquipRules& Rules);
	void RemoveAbilitySystem(FInventoryEquippedItem& Entry);

	void HandleCosmeticsLoaded(FInventoryItemHandle Item);
	/** Spawns pieces for every loaded entry, parents before their attachments. */
	void ApplyLoadedCosmetics();
	void ApplyCosmetics(FInventoryEquippedItem& Entry, const UInventoryItemCosmetics& Cosmetics);
	void RemoveCosmetics(FInventoryEquippedItem& Entry);
	/** Links every equipped item's layers again in equip order, so removing one layer doesn't hide another's. */
	void RelinkAnimLayers();

	USceneComponent* FindAttachParent(const UInventoryEquipmentPiece& Piece, const FInventoryEquippedItem& Entry) const;
	USkeletalMeshComponent* FindSkeletalMesh(FName NameOrTag) const;
	const FInventoryEquippedItem* FindEntry(FInventoryItemHandle Item) const;
	FInventoryEquippedItem* FindEntry(FInventoryItemHandle Item);
	UAbilitySystemComponent* GetAbilitySystem() const;

	UPROPERTY(Transient)
	TArray<FInventoryEquippedItem> Equipped;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	/** Slot to item, for items equipped in place. */
	TMap<FGameplayTag, FInventoryItemHandle> InPlaceItems;

	TWeakObjectPtr<UInventorySubsystem> Subsystem;
	bool bRefreshing = false;
	bool bRefreshPending = false;
};
