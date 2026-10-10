#include "HadesAbility.h"

#include "Hades/AbilitySystem/HadesAbilitySystemComponent.h"

UHadesAbility::UHadesAbility()
{
	bIsEnabled = true;
}

void UHadesAbility::CommitAbility()
{
	if (!IsActive())
	{
		ActivationState = EHadesAbilityActivationState::Active;
		OnActivated.Broadcast(this);
		K2_OnAbilityCommitted();
		if (bCooldownOnActivation) StartCooldown();
	}
}

void UHadesAbility::CommitAbilityEnd()
{
	if (IsActive())
	{
		ActivationState = EHadesAbilityActivationState::Idle;
		OnDeactivated.Broadcast(this);
		K2_OnAbilityEnded();
		if (bCooldownOnDeactivation) StartCooldown();
	}
}

void UHadesAbility::ForceEndAbility()
{
	if (IsActive())
	{
		CommitAbilityEnd();
	}
}

bool UHadesAbility::RequestActivateAbility()
{
	if (IsOnCooldown()) return false;
	if (!CanActivateAbility()) return false;
	
	K2_OnAbilityActivate();
	return true;
}

bool UHadesAbility::RequestDeactivateAbility(bool bForce)
{
	if (!bForce)
	{
		if (!IsActive()) return false;
	}
	
	K2_OnAbilityDeactivate();
	return true;
}

void UHadesAbility::OnEnableAbility()
{
	bIsEnabled = true;
	OnAbilityEnabled.Broadcast();
	
	K2_OnAbilityEnabled();
}

void UHadesAbility::OnDisableAbility()
{
	RequestDeactivateAbility(true);
	bIsEnabled = false;
	OnAbilityDisabled.Broadcast();
	
	K2_OnAbilityDisabled();
}


bool UHadesAbility::CanActivateAbility_Implementation() const
{
	return IsEnabled();
}


// Ability utility
UHadesAbilitySystemComponent* UHadesAbility::GetHadesAbilitySystemComponent() const
{
	return Cast<UHadesAbilitySystemComponent>(GetOuter());
}

AActor* UHadesAbility::GetOwner() const
{
	if (const UHadesAbilitySystemComponent* Comp = GetHadesAbilitySystemComponent())
	{
		return Comp->GetOwner();
	}
	return nullptr;
}

AController* UHadesAbility::GetController() const
{
	if (const UHadesAbilitySystemComponent* Comp = GetHadesAbilitySystemComponent())
	{
		return Comp->GetController(); 
	}
	return nullptr;
}


// Cooldown
bool UHadesAbility::HasCooldown() const
{
	return CooldownDuration > 0.0f;
}

float UHadesAbility::GetCooldownTotalDuration() const
{
	return FMath::Max(0.0f, CooldownDuration);
}

float UHadesAbility::GetCooldownTimeRemaining() const
{
	if (!bIsOnCooldown) return 0.0f;
	return FMath::Max(0.0f, GetCooldownTotalDuration() - CooldownElapsed);
}

float UHadesAbility::GetCooldownProgress() const
{
	if (!bIsOnCooldown) return 0.0f;
	const float Total = GetCooldownTotalDuration();
	if (Total <= 0.0f) return 0.0f;
	return FMath::Clamp(CooldownElapsed / Total, 0.0f, 1.0f);
}

void UHadesAbility::StartCooldown()
{
	const float Duration = GetCooldownTotalDuration();
	if (Duration > 0.0f)
	{
		bIsOnCooldown = true;
		CooldownElapsed = 0.0f;
	}
}

void UHadesAbility::RestartCooldown()
{
	StartCooldown();
}

void UHadesAbility::TickCooldown(float DeltaTime)
{
	if (!bIsOnCooldown) return;

	CooldownElapsed += DeltaTime;
	if (CooldownElapsed >= GetCooldownTotalDuration())
	{
		bIsOnCooldown = false;
		CooldownElapsed = 0.0f;
	}
}


UWorld* UHadesAbility::GetWorld() const
{
	if (const AActor* Owner = Cast<AActor>(GetOwner()))
	{
		return Owner->GetWorld();
	}

	return nullptr;
}