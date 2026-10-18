#pragma once

#include "CoreMinimal.h"

#include "Hades/AbilitySystem/HadesAbility.h"

#include "HadesAbility_Run.generated.h"


UCLASS(Blueprintable)
class HADES_API UHadesAbility_Run : public UHadesAbility
{
	GENERATED_BODY()

public:
	UHadesAbility_Run();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface
};
