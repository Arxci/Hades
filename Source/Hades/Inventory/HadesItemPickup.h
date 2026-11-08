#pragma once

#include "CoreMinimal.h"

#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "InventoryTypes.h"

#include "HadesItemPickup.generated.h"

class UHadesInteractableComponent;
class UHadesInventoryComponent;
class UInventoryComponent;
class UStaticMeshComponent;


/**
 * Items lying in the world, taken by interacting. The inventory component holds the real items, so their state
 * and anything nested inside them survive being dropped and picked up again.
 * Give the Blueprint a list container, plus starting items when it's placed in a level.
 * While the player looks at it without room for any of it, the prompt says so and it can't be taken.
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
	bool HasRoomFor(const UInventoryComponent& Taker) const;
	void TakePouch(UHadesInventoryComponent& Taker, FInventoryItemHandle Pouch);
	TArray<FInventoryItemHandle> GetItems() const;
	void ShowFirstItem();

	TWeakObjectPtr<UInventoryComponent> WatchedTaker;
	TSharedPtr<FStreamableHandle> MeshHandle;
};
