#include "HadesCharacterBase.h"

#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

#include "HadesCharacterMovementComponent.h"
#include "Hades/AbilitySystem/HadesAbilitySystemComponent.h"
#include "Hades/AbilitySystem/Attributes/HadesMovementSet.h"
#include "Hades/HadesGameplayTags.h"


AHadesCharacterBase::AHadesCharacterBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UHadesCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	HadesCharacterMovement = Cast<UHadesCharacterMovementComponent>(GetCharacterMovement());
	HadesAbilitySystemComponent = CreateDefaultSubobject<UHadesAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	MovementSet = CreateDefaultSubobject<UHadesMovementSet>(TEXT("MovementSet"));

	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
}

UAbilitySystemComponent* AHadesCharacterBase::GetAbilitySystemComponent() const
{
	return HadesAbilitySystemComponent;
}

void AHadesCharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	
	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->RefreshAbilityActorInfo();
	}
}

void AHadesCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && HadesAbilitySystemComponent)
	{
		FGameplayEffectContextHandle EffectContext = HadesAbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultEffects)
		{
			const FGameplayEffectSpecHandle EffectSpec = HadesAbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
			if (EffectSpec.IsValid())
			{
				HadesAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data);
			}
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
			{
				HadesAbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass));
			}
		}
	}
}

void AHadesCharacterBase::Run()
{
	if (HadesCharacterMovement)
	{
		HadesCharacterMovement->SetWantsToRun(true);
	}
}

void AHadesCharacterBase::StopRun() 
{
	if (HadesCharacterMovement)
	{
		HadesCharacterMovement->SetWantsToRun(false);
	}
}

void AHadesCharacterBase::OnStartRun()
{
	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->SetLooseGameplayTagCount(HadesGameplayTags::Character_State_Running, 1);
	}

	K2_OnStartRun();
}

void AHadesCharacterBase::OnEndRun()
{
	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->SetLooseGameplayTagCount(HadesGameplayTags::Character_State_Running, 0);
	}

	K2_OnEndRun();
}

void AHadesCharacterBase::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->SetLooseGameplayTagCount(HadesGameplayTags::Character_State_Crouching, 1);
	}
	
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AHadesCharacterBase::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (HadesAbilitySystemComponent)
	{
		HadesAbilitySystemComponent->SetLooseGameplayTagCount(HadesGameplayTags::Character_State_Crouching, 0);
	}
	
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

//Utility
bool AHadesCharacterBase::IsRunning() const
{
	return HadesCharacterMovement && HadesCharacterMovement->IsRunning();
}
