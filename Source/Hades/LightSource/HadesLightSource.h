#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "Equipment/InventoryEquipmentActorInterface.h"
#include "InventoryTypes.h"

#include "HadesLightSource.generated.h"

class AHadesLightSource;
class UInventoryEquipmentComponent;
class UInventorySubsystem;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHadesLightSourceChangedSignature, AHadesLightSource*, LightSource, bool, bIsOn);


/**
 * The light a light source item gives the player while it's in the light source slot, such as a flashlight, a lighter, a lantern or a flare.
 * An Actor piece of the item's cosmetics spawns it, attached to the player, and it goes when the item leaves the slot. Make a Blueprint of it
 * for each kind of light, holding its light components: a spot light under a Hades Flashlight Component for a flashlight, a flickering point
 * light and a flame for a lighter.
 * The item remembers whether the light is on, as its Light On Tag value, so it comes back as it was after it's put away, and widgets can show
 * it. With a Charge Tag, it uses that up while it's on, such as a battery or fuel, and goes out once it's empty, or is used up, as a flare is.
 * The player's key turns it on and off through the Toggle Light Source ability, which the item's equip rules grant.
 */
UCLASS(Abstract, Blueprintable)
class HADES_API AHadesLightSource : public AActor, public IInventoryEquipmentActorInterface
{
	GENERATED_BODY()

public:
	AHadesLightSource();

	//~ Begin AActor Interface
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

	//~ Begin IInventoryEquipmentActorInterface
	virtual void OnEquipped_Implementation(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle InItem, FGameplayTag Slot) override;
	virtual void OnUnequipped_Implementation(UInventoryEquipmentComponent* Equipment, FInventoryItemHandle InItem, FGameplayTag Slot) override;
	//~ End IInventoryEquipmentActorInterface

	/** Turns the light on or off. False when it couldn't turn on, such as with an empty battery. */
	UFUNCTION(BlueprintCallable, Category = "Light Source")
	bool SetLightOn(bool bOn);

	/** Turns the light off if it's on, and on if it's off and can be. False when it couldn't turn on. */
	UFUNCTION(BlueprintCallable, Category = "Light Source")
	bool ToggleLight();

	UFUNCTION(BlueprintPure, Category = "Light Source")
	bool IsLightOn() const { return bLightOn; }

	/** The item it's the light of. */
	UFUNCTION(BlueprintPure, Category = "Light Source")
	FInventoryItemHandle GetItem() const { return Item; }

	/** Whether it uses up charge while it's on: it has a Charge Tag and a Charge Per Second. */
	UFUNCTION(BlueprintPure, Category = "Light Source")
	bool UsesCharge() const;

	/** The charge it has left, in Charge Tag's units. 0 for a light without charge. */
	UFUNCTION(BlueprintPure, Category = "Light Source")
	float GetCharge() const;

	/** The charge it has left, from 0 to 1 of the item definition's Charge Tag value. 1 for a light without charge. */
	UFUNCTION(BlueprintPure, Category = "Light Source")
	float GetChargeFraction() const;

	/** Who holds it: the actor it's attached to, or else its owner. */
	UFUNCTION(BlueprintPure, Category = "Light Source")
	AActor* GetHolder() const;

	/** The light turned on or off. */
	UPROPERTY(BlueprintAssignable, Category = "Light Source")
	FHadesLightSourceChangedSignature OnLightChanged;

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** Whether it can turn on now. By default, whether it has charge left, when it uses charge. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Light Source")
	bool CanTurnOn() const;

	/**
	 * Shows the light on or off. By default it turns this actor's light components on or off, and its Hades Flashlight Components, which draw
	 * a flashlight's beam. Override it to do more, such as show a lighter's flame or play a click, and call the parent to keep that.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Light Source")
	void ApplyLightOn(bool bOn);

	/** It was asked to turn on but couldn't, such as with an empty battery, such as to play a dry click. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Light Source")
	void OnTurnOnFailed();

	/** Its charge ran out, before it went out, or was used up with Use Up When Empty. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Light Source")
	void OnChargeRanOut();

	/**
	 * The tag value of its item that's 1 while the light is on, so the item remembers it, and widgets can show it. The item definition's own
	 * value, 0 unless it's given one, is how the light is found.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Light Source")
	FGameplayTag LightOnTag;

	/** A tag value of its item that it uses up while it's on, such as Item.Stat.Charge for a battery or fuel. None never runs out. */
	UPROPERTY(EditDefaultsOnly, Category = "Light Source|Charge")
	FGameplayTag ChargeTag;

	/** How much of Charge Tag it uses each second while it's on, in game time, so none goes while the game is paused. */
	UPROPERTY(EditDefaultsOnly, Category = "Light Source|Charge", meta = (ClampMin = "0"))
	float ChargePerSecond = 0.0f;

	/** The item's charge is written in steps of this much, and as the light goes out, so widgets showing it don't change every frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Light Source|Charge", meta = (ClampMin = "0.01"))
	float ChargeStep = 1.0f;

	/** Once its charge runs out, one of the item is used up, such as a flare that burns out. Off, the light only goes out. */
	UPROPERTY(EditDefaultsOnly, Category = "Light Source|Charge")
	bool bUseUpWhenEmpty = false;

private:
	UInventorySubsystem* GetInventory() const;

	/** Stores whether the light is on on the item, when that changed. */
	void StoreLightOn(bool bOn) const;

	/** Writes the charge used since it was last written to the item. */
	void CommitCharge();
	void HandleChargeRanOut();

	/** Ticks only while it's on and uses charge. */
	void RefreshTickEnabled();

	FInventoryItemHandle Item;
	TWeakObjectPtr<UInventoryEquipmentComponent> EquipmentComponent;
	bool bLightOn = false;

	/** Charge used and not yet written to the item. */
	float PendingDrain = 0.0f;
};
