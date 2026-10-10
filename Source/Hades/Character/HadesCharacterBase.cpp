#include "HadesCharacterBase.h"

#include "HadesCharacterMovementComponent.h"
#include "Hades/AbilitySystem/HadesAbilitySystemComponent.h"
#include "Hades/AbilitySystem/HadesAbility.h"


AHadesCharacterBase::AHadesCharacterBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UHadesCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	HadesCharacterMovement = Cast<UHadesCharacterMovementComponent>(GetCharacterMovement());
	HadesAbilitySystemComponent = CreateDefaultSubobject<UHadesAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
}

void AHadesCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->InitAbilityActorInfo(NewController);
	}
}

void AHadesCharacterBase::UnPossessed()
{
	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->InitAbilityActorInfo(nullptr);
	}
	Super::UnPossessed();
}

void AHadesCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Granted once per lifetime rather than per possession, so re-possessing doesn't stack grant counts.
	if (HadesAbilitySystemComponent)
	{
		for (const TSubclassOf<UHadesAbility>& AbilityClass : DefaultAbilities)
		{
			HadesAbilitySystemComponent->GiveAbility(AbilityClass);
		}
	}
	
	if (HadesCharacterMovement)
	{
		HadesCharacterMovement->OnRunStart.AddDynamic(this, &AHadesCharacterBase::OnStartRun);
		HadesCharacterMovement->OnRunEnd.AddDynamic(this, &AHadesCharacterBase::OnEndRun);
	}
}

void AHadesCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HadesCharacterMovement)
	{
		HadesCharacterMovement->OnRunStart.RemoveDynamic(this, &AHadesCharacterBase::OnStartRun);
		HadesCharacterMovement->OnRunEnd.RemoveDynamic(this, &AHadesCharacterBase::OnEndRun);
	}

	Super::EndPlay(EndPlayReason);
}

void AHadesCharacterBase::Run() const
{
	if (HadesCharacterMovement)
	{
		HadesCharacterMovement->StartRunning();
	}
}

void AHadesCharacterBase::StopRun() const
{
	if (HadesCharacterMovement)
	{
		HadesCharacterMovement->StopRunning();
	}
}

void AHadesCharacterBase::OnStartRun()
{

}

void AHadesCharacterBase::OnEndRun()
{

}

//Utility
bool AHadesCharacterBase::GetIsCrouched() const
{
	if (HadesCharacterMovement)
	{
		return HadesCharacterMovement->IsCrouching();
	}
	return false;	
}

bool AHadesCharacterBase::GetIsRunning() const
{
	if (HadesCharacterMovement)
	{
		return HadesCharacterMovement->IsRunning();
	}
	return false;	
}

bool AHadesCharacterBase::GetIsGrounded() const
{
	if (HadesCharacterMovement)
	{
		return HadesCharacterMovement->IsGrounded();
	}
	return true;	
}