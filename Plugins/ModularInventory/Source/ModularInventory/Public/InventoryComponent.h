#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"

#include "InventoryComponent.generated.h"

class UInventoryComponent;
class UInventoryContainerLayout;
class UInventoryLootTable;
class UInventoryRecipe;
class UInventoryRecipeBook;
class UInventorySubsystem;
struct FInventoryChangeSet;


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryContainerConfig
{
	GENERATED_BODY()

	/** Tells this component's containers apart, e.g. Container.Pockets or Container.Equipment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryContainerLayout> Layout;
};


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryStartingEquipment
{
	GENERATED_BODY()

	/** A slot in one of the component's equipment containers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FGameplayTag Slot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FInventoryItemSpec Item;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryComponentChangedSignature, UInventoryComponent*, Inventory);


/**
 * Gives an actor its own root containers: a player's pockets and equipment slots, a chest, a pickup lying in the world.
 * The items live in the world's inventory registry; this component creates the containers on begin play and destroys them on end play.
 */
UCLASS(Blueprintable, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class MODULARINVENTORY_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UFUNCTION(BlueprintPure, Category = "Inventory", meta = (DefaultToSelf = "Actor"))
	static UInventoryComponent* FindInventoryComponent(const AActor* Actor);

	// Containers
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetContainers() const { return RootContainers; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FInventoryContainerHandle GetContainer(FGameplayTag Tag) const;

	/** Where new items go, in order: root containers, then containers inside items, breadth first. Skips equipment slots but includes what equipped items carry, like a worn backpack. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetStorageContainers() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryContainerHandle> GetEquipmentContainers() const;

	/** Whether the container is one of this component's, or nested anywhere inside them. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool OwnsContainer(FInventoryContainerHandle Container) const;

	/** What begin play will add, for editor previews such as a pickup showing its item. */
	const TArray<FInventoryItemSpec>& GetStartingItems() const { return StartingItems; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool OwnsItem(FInventoryItemHandle Item) const;

	// Items
	/** Adds to the storage containers, topping up stacks first. Returns how many were added; without Allow Partial that is all or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(const FInventoryItemSpec& Spec, bool bAllowPartial, TArray<FInventoryItemHandle>& OutItems);

	/** Rolls the table and adds the results. With Allow Partial, whatever doesn't fit is discarded; otherwise all or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddLoot(const UInventoryLootTable* LootTable, const FGameplayTagContainer& ContextTags, bool bAllowPartial = true);

	/** Moves an item from anywhere into this component's storage, topping up stacks first. Count 0 moves the whole stack. With Allow Partial, what doesn't fit stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Moves an item into Target's storage. Count 0 moves the whole stack. With Allow Partial, what doesn't fit stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TransferItem(FInventoryItemHandle Item, UInventoryComponent* Target, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Removes up to Count matching items from everything this component holds; 0 removes them all. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItems(const FInventoryItemQuery& Query, int32 Count = 0);

	/** Searches everything this component holds, equipment and nested containers included. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemHandle> FindItems(const FInventoryItemQuery& Query) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItems(const FInventoryItemQuery& Query) const;

	// Crafting. Ingredients come from storage, and the owner's ability system tags join the context tags.
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	bool CanCraft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times = 1) const;

	/** All or nothing: if the results don't fit, the ingredients are kept. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool Craft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	TArray<UInventoryRecipe*> GetCraftableRecipes(const UInventoryRecipeBook* Book, const FGameplayTagContainer& ContextTags) const;

	/** Fires once per committed change touching any of this component's containers, nested ones included. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryComponentChangedSignature OnInventoryChanged;

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (TitleProperty = "Tag"))
	TArray<FInventoryContainerConfig> Containers;

	/**
	 * Keeps the cosmetics of everything held here loaded, so menus and equipping never wait on them.
	 * Turn it on for the player; chests and pickups are better off loading on demand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	bool bPreloadCosmetics = false;

	/** Added on begin play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items")
	TArray<FInventoryItemSpec> StartingItems;

	/** Placed straight into equipment slots on begin play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items", meta = (TitleProperty = "Slot"))
	TArray<FInventoryStartingEquipment> StartingEquipment;

	/** Rolled and added on begin play, e.g. what a drawer holds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items")
	TObjectPtr<UInventoryLootTable> StartingLoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|Starting Items")
	FGameplayTagContainer StartingLootContext;

private:
	UInventorySubsystem* GetSubsystem() const { return Subsystem.Get(); }
	FGameplayTagContainer GatherContextTags(const FGameplayTagContainer& ExtraTags) const;
	void HandleRegistryChanged(const FInventoryChangeSet& Changes);
	void RefreshCosmeticsPreload();

	TArray<FInventoryContainerHandle> RootContainers;
	TWeakObjectPtr<UInventorySubsystem> Subsystem;
	FDelegateHandle RegistryChangedHandle;

	TSet<FSoftObjectPath> PreloadedCosmetics;
	TSharedPtr<FStreamableHandle> CosmeticsHandle;
};
