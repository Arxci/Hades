#pragma once

#include "CoreMinimal.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

#include "HadesHealthSet.generated.h"


/**
 * A character's health, as Lyra keeps it: Health from 0 to Max Health, changed by gameplay effects. Effects that hurt or heal add to the
 * Damage and Healing meta attributes, which are taken off or added to Health, kept within 0 and Max Health, so one effect works whatever the
 * character's health is: a herb's Inventory Item Use can apply an effect adding 50 to Healing.
 * For testing, the console commands Hades.Health.Damage, Hades.Health.Heal and Hades.Health.Set change the local player's health.
 */
UCLASS()
class HADES_API UHadesHealthSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UHadesHealthSet();

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHadesHealthSet, Health)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health)

	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHadesHealthSet, MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth)

	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHadesHealthSet, Healing)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Healing)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Healing)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Healing)

	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHadesHealthSet, Damage)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Damage)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Damage)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Damage)

private:
	/** Keeps Health within 0 and Max Health, and Max Health above 0. */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	/** What the character can take: 0 is dead. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	/** Meta attribute: added to Health as an effect executes, then cleared. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Healing;

	/** Meta attribute: taken off Health as an effect executes, then cleared. */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Damage;
};
