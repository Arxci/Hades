#pragma once

#include "CoreMinimal.h"

#include "AbilitySystemComponent.h"

#include "HadesAbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HADES_API UHadesAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);

	/** Forgets held input and cancels the abilities it keeps active, toggled ones included. For when the pawn loses its player. */
	void ClearAbilityInput();

protected:
	//~ Begin UAbilitySystemComponent Interface
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	//~ End UAbilitySystemComponent Interface

private:
	void TryActivateHeldAbilities();

	/** Abilities that may restart while their input is held, oldest press first. */
	TArray<FGameplayAbilitySpecHandle> HeldInputSpecHandles;

	bool bActivatingHeldAbilities = false;
};
