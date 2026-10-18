#pragma once

#include "CoreMinimal.h"

#include "AbilitySystemComponent.h"

#include "HadesAbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HADES_API UHadesAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Ability System")
	void ToggleAbilitiesByTag(const FGameplayTagContainer& AbilityTags);
};
