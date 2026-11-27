#pragma once

#include "CoreMinimal.h"

#include "Hades/AbilitySystem/HadesAbility.h"

#include "InventoryTypes.h"

#include "HadesItemAbility.generated.h"

class UHadesInventoryComponent;
class UInventoryEquipmentComponent;


/**
 * An ability an item grants while it's equipped, from the Abilities of its equip rules: what the player does with what they hold, such as
 * turning a light on, firing, reloading or inspecting a weapon. It knows the item that granted it, and the actors that item's equipment
 * pieces spawned, such as the gun in the player's hands, to read and change them.
 * Each is activated by its input tag, as any Hades ability: its own Input Tag, or the Input Tag of the equip rules' grant, which lets one
 * ability class answer a different key on each item. The hero's input config maps each input tag to an Enhanced Input action, which players
 * can rebind.
 */
UCLASS(Abstract)
class HADES_API UHadesItemAbility : public UHadesAbility
{
	GENERATED_BODY()

public:
	/** The item that granted this ability, while it's equipped. */
	UFUNCTION(BlueprintPure, Category = "Hades Ability|Item")
	FInventoryItemHandle GetSourceItem() const;

	UFUNCTION(BlueprintPure, Category = "Hades Ability|Item")
	UInventoryEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "Hades Ability|Item")
	UHadesInventoryComponent* GetHadesInventoryComponent() const;

	/** The first actor of the class that the source item's equipment pieces spawned, such as the light or the gun it puts in the player's hands. */
	UFUNCTION(BlueprintPure, Category = "Hades Ability|Item", meta = (DeterminesOutputType = "ActorClass"))
	AActor* FindSpawnedActor(TSubclassOf<AActor> ActorClass) const;

	template <typename T>
	T* FindSpawnedActor() const
	{
		return Cast<T>(FindSpawnedActor(T::StaticClass()));
	}
};
