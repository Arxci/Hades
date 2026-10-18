#pragma once

#include "CoreMinimal.h"

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "HadesMovementSet.generated.h"



UCLASS()
class HADES_API UHadesMovementSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UHadesMovementSet();

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	//~ End UAttributeSet Interface
	
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UHadesMovementSet, MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MoveSpeedMultiplier)
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MoveSpeedMultiplier)

private:
	UPROPERTY(BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MoveSpeedMultiplier;
};
