#pragma once

#include "CoreMinimal.h"

#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"

#include "HadesItemPickup.generated.h"

class UHadesInteractableComponent;
class UInventoryComponent;
class UStaticMeshComponent;


/**
 * Items lying in the world, taken by interacting. The inventory component holds the real items, so their state
 * and anything nested inside them survive being dropped and picked up again.
 * Give the Blueprint a list container, plus starting items when it's placed in a level.
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

	void ShowFirstItem();

	TSharedPtr<FStreamableHandle> MeshHandle;
};
