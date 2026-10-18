#pragma once

#include "CoreMinimal.h"

#include "Abilities/GameplayAbility.h"

#include "HadesAbility.generated.h"

class AHadesCharacterBase;


UCLASS(Abstract)
class HADES_API UHadesAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UHadesAbility();
	
	UFUNCTION(BlueprintPure, Category = "Hades Ability")
	AHadesCharacterBase* GetHadesCharacterFromActorInfo() const;
};
