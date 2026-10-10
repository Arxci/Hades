#include "HadesAbility_Run.h"

#include "Hades/AbilitySystem/HadesAbilitySystemComponent.h"
#include "Hades/Character/HadesCharacterBase.h"
#include "Hades/Character/HadesCharacterMovementComponent.h"
#include "Hades/HadesGameplayTags.h"

UHadesAbility_Run::UHadesAbility_Run()
{
	AbilityTags.AddTag(HadesGameplayTags::Ability_Run);
	CancelAbilitiesWithTags.AddTag(HadesGameplayTags::Ability_Crouch);
	ActivationOwnedTags.AddTag(HadesGameplayTags::Character_State_Running);
	bCooldownOnDeactivation = false;
}

void UHadesAbility_Run::TickAbility(float DeltaTime)
{
	Super::TickAbility(DeltaTime);

	if (IsOnCooldown()) return;
	
	if (CanCharacterRun())
	{
		if (!IsActive())
		{
			CommitAbility();
		}
	}
	else if (CanCharacterWalk())
	{
		if (IsActive())
		{
			if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
			{
				ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Walk);
			}
			CommitAbilityEnd();
		}
	}
	else if (IsActive())
	{
		if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
		{
			if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_Run))
			{
				ASC->AddLooseGameplayTag(HadesGameplayTags::Ability_Intent_Run);
			}
		}
		CommitAbilityEnd();
	}
}

bool UHadesAbility_Run::RequestActivateAbility()
{
	if (!Super::RequestActivateAbility()) return false;
	
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		//Intent tags are flags; a second ref would survive the single remove in RequestDeactivateAbility.
		if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_Run))
		{
			ASC->AddLooseGameplayTag(HadesGameplayTags::Ability_Intent_Run);
		}
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Walk);
	}
	
	return true;
}

bool UHadesAbility_Run::RequestDeactivateAbility(bool bForce)
{
	//Always clear run intent due to possibility of partially active due to IsMovingForward logic checks.
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Run);
	}
	
	if (!Super::RequestDeactivateAbility(bForce)) return false;

	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_Walk))
		{
			ASC->AddLooseGameplayTag(HadesGameplayTags::Ability_Intent_Walk);
		}
	}
	
	return true;
}

bool UHadesAbility_Run::CanCharacterRun() const
{
	if (const AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		if (!Char->GetHadesCharacterMovement()->IsGrounded()) return false;
		if (!Char->GetHadesCharacterMovement()->IsMovingForward(BoostThreshold)) return false;
	}
	
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		const bool bRunningOrIntending =
			ASC->HasTag(HadesGameplayTags::Ability_Run) ||
			ASC->HasTag(HadesGameplayTags::Ability_Intent_Run);

		const bool bCancelTagSnapshot = ASC->HasAnyTags(CancelAbilitiesWithTags);
		const bool bWantingWalk = ASC->HasTag(HadesGameplayTags::Ability_Intent_Walk);

		if (!bRunningOrIntending || bCancelTagSnapshot || bWantingWalk) return false;
	}

	return true;
}

bool UHadesAbility_Run::CanCharacterWalk() const
{
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_Walk)) return false;
	}

	if (IsActive()) return true;

	return false;
}

bool UHadesAbility_Run::IsActivationRequested() const
{
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		//Run intent survives stopping and resumes on forward movement; walk intent means the run was already let go.
		if (ASC->HasTag(HadesGameplayTags::Ability_Intent_Run)) return true;
		if (ASC->HasTag(HadesGameplayTags::Ability_Intent_Walk)) return false;
	}
	return Super::IsActivationRequested();
}

bool UHadesAbility_Run::CanTick()
{
	bool bCanTick = false;
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		bCanTick = ASC->HasTag(HadesGameplayTags::Ability_Intent_Run) || ASC->HasTag(HadesGameplayTags::Ability_Intent_Walk) || IsActive();
	}
	return bCanTick;
}

void UHadesAbility_Run::CommitAbility()
{
	Super::CommitAbility();
	
	if (AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		Char->Run();
	}
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Run);
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Walk);
	}
}

void UHadesAbility_Run::CommitAbilityEnd()
{
	Super::CommitAbilityEnd();
	
	if (AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		Char->StopRun();
	}
}

void UHadesAbility_Run::ForceEndAbility()
{
	Super::ForceEndAbility();
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Run);
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Walk);
	}

	if (AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		Char->StopRun();
	}
}