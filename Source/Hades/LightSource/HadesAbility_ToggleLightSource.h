#pragma once

#include "CoreMinimal.h"

#include "Hades/AbilitySystem/HadesItemAbility.h"

#include "HadesAbility_ToggleLightSource.generated.h"


/**
 * Turns the light of the light source item that granted it on or off: grant it in the light source's equip rules. Its Input Tag is
 * Input.LightSource, so the key the hero's input config maps to it, which the player can rebind, toggles the light. Without a light, such as
 * while the item's light is still loading, it does nothing.
 */
UCLASS()
class HADES_API UHadesAbility_ToggleLightSource : public UHadesItemAbility
{
	GENERATED_BODY()

public:
	UHadesAbility_ToggleLightSource();

	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility Interface
};
