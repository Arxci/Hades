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

	/** Whether Add Item would succeed, without adding anything. With Allow Partial, whether any of it fits. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanAddItem(const FInventoryItemSpec& Spec, bool bAllowPartial) const;

	/** Rolls the table and adds the results. With Allow Partial, whatever doesn't fit is discarded; otherwise all or nothing. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddLoot(const UInventoryLootTable* LootTable, const FGameplayTagContainer& ContextTags, bool bAllowPartial = true);

	/** Moves an item from anywhere into this component's storage, topping up stacks first. Count 0 moves the whole stack. With Allow Partial, what doesn't fit stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Whether Take Item would succeed, without moving anything, such as to grey out a pickup the player has no room for. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanTakeItem(FInventoryItemHandle Item, int32 Count, bool bAllowPartial) const;

	/** Moves an item into Target's storage. Count 0 moves the whole stack. With Allow Partial, what doesn't fit stays behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TransferItem(FInventoryItemHandle Item, UInventoryComponent* Target, int32 Count, bool bAllowPartial, EInventoryError& OutError);

	/** Takes as much as fits of everything in Container, such as a chest's, into storage. True when nothing is left behind. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool TakeAll(FInventoryContainerHandle Container);

	/** Removes up to Count matching items from everything this component holds; 0 removes them all. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItems(const FInventoryItemQuery& Query, int32 Count = 0);

	/** Searches everything this component holds, equipment and nested containers included. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemHandle> FindItems(const FInventoryItemQuery& Query) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItems(const FInventoryItemQuery& Query) const;

	/** The sum of a tag value over everything this component holds, each times its count, such as the weight carried. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetTotalTagValue(FGameplayTag Tag) const;

	// Crafting. Ingredients come from storage, and the owner's ability system tags join the context tags.
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	bool CanCraft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times = 1) const;

	/** All or nothing: if the results don't fit, the ingredients are kept. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool Craft(const UInventoryRecipe* Recipe, const FGameplayTagContainer& ContextTags, int32 Times, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	TArray<UInventoryRecipe*> GetCraftableRecipes(const UInventoryRecipeBook* Book, const FGameplayTagContainer& ContextTags) const;

	/** The recipes in Book with an ingredient this item can be, such as to offer Combine for it, or to show what it makes. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	TArray<UInventoryRecipe*> GetRecipesUsing(const UInventoryRecipeBook* Book, FInventoryItemHandle Item) const;

	/** The first recipe in Book made from exactly these items held here, every one of them used, such as two herbs combined. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Crafting")
	UInventoryRecipe* FindRecipeMadeFrom(const UInventoryRecipeBook* Book, const TArray<FInventoryItemHandle>& Items, const FGameplayTagContainer& ContextTags) const;

	/** Crafts once taking ingredients only from these items held here, such as the ones a player chose to combine. The results go to storage. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Crafting")
	bool CraftFromItems(const UInventoryRecipe* Recipe, const TArray<FInventoryItemHandle>& Items, const FGameplayTagContainer& ContextTags, TArray<FInventoryItemHandle>& OutItems, EInventoryError& OutError);

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
