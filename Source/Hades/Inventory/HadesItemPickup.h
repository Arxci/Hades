#pragma once

#include "CoreMinimal.h"

#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"

#include "HadesItemPickup.generated.h"

class UHadesInteractableComponent;
class UInventoryComponent;
class UInventoryItemDefinition;
class UStaticMeshComponent;


/**
 * Items lying in the world, taken by interacting. The inventory component holds the real items, so their state
 * and anything nested inside them survive being dropped and picked up again.
 * Give the Blueprint a list container, plus starting items when it's placed in a level.
 * While the player looks at it without room for any of it, the prompt says so and it can't be taken. An item that only fits an equipment slot
 * the player has filled already, of a container that takes new items, such as a second light source, is swapped instead: the prompt says
 * Swap, and the item the player had is left here in its place.
 */
UCLASS(Blueprintable)
class HADES_API AHadesItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AHadesItemPickup();

	UFUNCTION(BlueprintPure, Category = "Pickup")
	UInventoryComponent* GetInventory() const { return Inventory; }

protected:
	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** Left empty, it shows the first item's world mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UHadesInteractableComponent> Interactable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UInventoryComponent> Inventory;

	/** What interacting does, for the prompt, while the player has room for any of it. */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	FText TakeText;

	/** What interacting does while the player has no room for any of it, but can swap it for what's in one of their equipment slots. */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	FText SwapText;

private:
	/** Takes as much as fits; the pickup goes away once it's empty. */
	UFUNCTION()
	void HandleInteractionCompleted(AActor* Interactor);

	UFUNCTION()
	void HandleStateTagChanged(FGameplayTag Tag, bool bAdded);

	/** Its own inventory, or the inventory of the player looking at it. */
	UFUNCTION()
	void HandleInventoryChanged(UInventoryComponent* ChangedInventory);

	void WatchTaker(UInventoryComponent* NewTaker);
	void RefreshRoom();
	TArray<FInventoryItemHandle> GetItems() const;
	void ShowFirstItem();

	/**
	 * The taker's equipment slot the item would be swapped into: one its equip rules allow, of an equipment container that takes new items,
	 * holding another item, which would come here in its place. None when the item can't be swapped.
	 */
	FGameplayTag FindSwapSlot(const UInventoryComponent& Taker, FInventoryItemHandle Item) const;

	TWeakObjectPtr<UInventoryComponent> WatchedTaker;
	TSharedPtr<FStreamableHandle> MeshHandle;

	/** The item definition whose world mesh shows, so the mesh changes once another item comes first, such as one swapped in. */
	TWeakObjectPtr<const UInventoryItemDefinition> ShownMeshDefinition;
};
