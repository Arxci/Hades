#include "HadesAbility_Crouch.h"

#include "GameFramework/Character.h"

#include "Hades/AbilitySystem/HadesAbilitySystemComponent.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Character/HadesCharacterBase.h"
#include "Hades/Character/HadesCharacterMovementComponent.h"


UHadesAbility_Crouch::UHadesAbility_Crouch()
{
	AbilityTags.AddTag(HadesGameplayTags::Ability_Crouch);
	CancelAbilitiesWithTags.AddTag(HadesGameplayTags::Ability_Run);
	ActivationOwnedTags.AddTag(HadesGameplayTags::Character_State_Crouching);
	bCooldownOnActivation = true;
	bCooldownOnDeactivation = true;
}

void UHadesAbility_Crouch::TickAbility(float DeltaTime)
{
	Super::TickAbility(DeltaTime);
	
	if (IsOnCooldown()) return;
	
	if (CanCharacterCrouch())
	{
		if (!IsActive())
		{
			CommitAbility();
		}
	}
	else if (CanCharacterUnCrouch())
	{
		if (IsActive())
		{
			CommitAbilityEnd();
		}
	}
}

bool UHadesAbility_Crouch::RequestActivateAbility()
{
	if (!Super::RequestActivateAbility()) return false;
	
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		ASC->AddLooseGameplayTag(HadesGameplayTags::Ability_Intent_Crouch);
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_UnCrouch);
	}
	
	return true;
}

bool UHadesAbility_Crouch::RequestDeactivateAbility(bool bForce)
{
	if (!Super::RequestDeactivateAbility(bForce)) return false;
	
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_UnCrouch))
		{
			ASC->AddLooseGameplayTag(HadesGameplayTags::Ability_Intent_UnCrouch);
		}
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Crouch);
	}
	
	return true;
}

bool UHadesAbility_Crouch::CanCharacterCrouch() const
{
	if (const AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		if (!Char->GetHadesCharacterMovement()->IsGrounded()) return false;
	}
	
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		const bool bCrouchingOrIntending =
			ASC->HasTag(HadesGameplayTags::Ability_Crouch) ||
			ASC->HasTag(HadesGameplayTags::Ability_Intent_Crouch);

		const bool bCancelTagSnapshot = ASC->HasAnyTags(CancelAbilitiesWithTags);
		const bool bWantingUncrouch = ASC->HasTag(HadesGameplayTags::Ability_Intent_UnCrouch);

		if (!bCrouchingOrIntending || bCancelTagSnapshot || bWantingUncrouch) return false;
	}
	
	return true;
}

bool UHadesAbility_Crouch::CanCharacterUnCrouch() const
{
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_UnCrouch)) return false;
	}

	if (const AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		if (!Char->GetHadesCharacterMovement()->CanStand()) return false;
	}

	if (IsActive()) return true;

	return false;
}

bool UHadesAbility_Crouch::CanTick()
{
	bool bCanTick = false;
	if (const UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		bCanTick = ASC->HasTag(HadesGameplayTags::Ability_Intent_Crouch) || ASC->HasTag(HadesGameplayTags::Ability_Intent_UnCrouch) || IsActive();
	}
	return bCanTick;
}

void UHadesAbility_Crouch::CommitAbility()
{
	Super::CommitAbility();
	
	if (AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		Char->Crouch();
	}
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Crouch);
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_UnCrouch);
	}
}

void UHadesAbility_Crouch::CommitAbilityEnd()
{
	Super::CommitAbilityEnd();

	if (AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner()))
	{
		Char->UnCrouch();
	}
	if (UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent())
	{
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_UnCrouch);
		ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Crouch);
	}
}

void UHadesAbility_Crouch::ForceEndAbility()
{
	if (IsActive())
	{
		AHadesCharacterBase* Char = Cast<AHadesCharacterBase>(GetOwner());
		UHadesAbilitySystemComponent* ASC = GetHadesAbilitySystemComponent();
		const bool bCanStand = Char && Char->GetHadesCharacterMovement()
			&& Char->GetHadesCharacterMovement()->CanStand();

		if (!bCanStand)
		{
			if (ASC)
			{
				ASC->RemoveLooseGameplayTag(HadesGameplayTags::Ability_Intent_Crouch);
				if (!ASC->HasTag(HadesGameplayTags::Ability_Intent_UnCrouch))
				{
					ASC->AddLooseGameplayTag(HadesGameplayTags::Ability_Intent_UnCrouch);
				}
			}
			return;
		}

	CommitAbilityEnd();
	}
}

