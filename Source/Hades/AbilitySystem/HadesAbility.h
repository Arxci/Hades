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

	//~ Begin UGameplayAbility Interface
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	//~ End UGameplayAbility Interface

	UFUNCTION(BlueprintPure, Category = "Hades Ability")
	AHadesCharacterBase* GetHadesCharacterFromActorInfo() const;

	const FGameplayTag& GetInputTag() const { return InputTag; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/** Ends the ability when its input is released, or on the next press if the player switched this input to toggle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bEndOnInputRelease = false;

private:
	bool IsInputToggled(const FGameplayAbilityActorInfo* ActorInfo) const;
};
