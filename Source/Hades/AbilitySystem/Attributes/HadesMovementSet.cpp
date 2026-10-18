#include "HadesMovementSet.h"


UHadesMovementSet::UHadesMovementSet()
{
	InitMoveSpeedMultiplier(1.0f);
}

void UHadesMovementSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetMoveSpeedMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}
