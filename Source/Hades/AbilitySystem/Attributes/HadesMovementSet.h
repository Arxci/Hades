#pragma once

#include "CoreMinimal.h"

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "HadesMovementSet.generated.h"


/**
 * Movement attributes. Gameplay effects modify these (a slow multiplies MoveSpeedMultiplier by 0.5, for example)
 * and the character movement component applies the result to its own walk, run and crouch speeds.
 */
UCLASS()
class HADES_API UHadesMovementSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UHadesMovementSet();

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	//~ End UAttributeSet Interface

	// GetMoveSpeedMultiplierAttribute(), GetMoveSpeedMultiplier(), SetMoveSpeedMultiplier() and InitMoveSpeedMultiplier().
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHadesMovementSet, MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MoveSpeedMultiplier)

private:
	/** Scales every movement speed. 1 is normal speed, 0.5 is half speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MoveSpeedMultiplier;
};
