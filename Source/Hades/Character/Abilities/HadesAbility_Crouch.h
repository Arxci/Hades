#pragma once

#include "CoreMinimal.h"

#include "Hades/AbilitySystem/HadesAbility.h"

#include "HadesAbility_Crouch.generated.h"


UCLASS()
class HADES_API UHadesAbility_Crouch : public UHadesAbility
{
	GENERATED_BODY()

public:
	UHadesAbility_Crouch();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface
};
