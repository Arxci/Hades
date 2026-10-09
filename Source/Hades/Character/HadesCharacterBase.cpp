#include "HadesCharacterBase.h"

#include "HadesCharacterMovementComponent.h"


AHadesCharacterBase::AHadesCharacterBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UHadesCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	HadesCharacterMovement = Cast<UHadesCharacterMovementComponent>(GetCharacterMovement());

	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
}
