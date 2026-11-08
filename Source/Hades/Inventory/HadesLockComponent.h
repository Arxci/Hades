#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "InventoryTypes.h"

#include "HadesLockComponent.generated.h"

class UInventoryItemDefinition;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHadesUnlockedSignature, AActor*, Unlocker);


/**
 * Keeps something locked, such as a door, until the player brings its key. Call Try Unlock from the owner's interaction.
 * Each unlock uses up one of the key's Item.Stat.Uses, and the key is discarded once none are left. A key without Uses is kept.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHadesLockComponent();

	/** True when it's unlocked, now or before. Otherwise the player is told it's locked. */
	UFUNCTION(BlueprintCallable, Category = "Lock")
	bool TryUnlock(AActor* Unlocker);

	UFUNCTION(BlueprintPure, Category = "Lock")
	bool IsLocked() const { return bLocked; }

	UPROPERTY(BlueprintAssignable, Category = "Lock")
	FHadesUnlockedSignature OnUnlocked;

protected:
	UPROPERTY(EditAnywhere, Category = "Lock")
	TObjectPtr<UInventoryItemDefinition> Key;

	UPROPERTY(EditAnywhere, Category = "Lock")
	bool bLocked = true;

	/** Shown when the player tries it without the key. */
	UPROPERTY(EditAnywhere, Category = "Lock")
	FText LockedText;

private:
	void UseUpKey(AActor* Unlocker, FInventoryItemHandle KeyItem);
};
