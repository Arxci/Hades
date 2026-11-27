#include "HadesHealthSet.h"

#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffectExtension.h"
#include "HAL/IConsoleManager.h"

#include "Hades/Hades.h"

UHadesHealthSet::UHadesHealthSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitHealing(0.0f);
	InitDamage(0.0f);
}

void UHadesHealthSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UHadesHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHadesHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UHadesHealthSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// A lower Max Health takes Health down with it.
	UAbilitySystemComponent* AbilitySystem = GetOwningAbilitySystemComponent();
	if (Attribute == GetMaxHealthAttribute() && AbilitySystem && GetHealth() > NewValue)
	{
		AbilitySystem->SetNumericAttributeBase(GetHealthAttribute(), FMath::Min(AbilitySystem->GetNumericAttributeBase(GetHealthAttribute()), NewValue));
	}
}

void UHadesHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Damage and healing are meta attributes: taken off or added to Health, then cleared for the next effect.
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float Taken = GetDamage();
		SetDamage(0.0f);
		SetHealth(FMath::Clamp(GetHealth() - Taken, 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		const float Healed = GetHealing();
		SetHealing(0.0f);
		SetHealth(FMath::Clamp(GetHealth() + Healed, 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

// Console commands, to see health widgets react without anything that hurts or heals yet.
#if !UE_BUILD_SHIPPING
namespace
{
	UAbilitySystemComponent* FindLocalPlayerAbilitySystem(const UWorld* World)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		UAbilitySystemComponent* AbilitySystem = Player ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player->GetPawn()) : nullptr;
		if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(UHadesHealthSet::GetHealthAttribute()))
		{
			UE_LOG(LogHadesAbility, Warning, TEXT("[Health] The local player's pawn has no Hades Health Set to change."));
			return nullptr;
		}
		return AbilitySystem;
	}

	/** Sets the local player's health to what Change makes of it, the first argument being the amount, or Default without one. */
	void ChangeHealth(const TArray<FString>& Args, const UWorld* World, float Default, TFunctionRef<float(float Health, float Amount)> Change)
	{
		if (UAbilitySystemComponent* AbilitySystem = FindLocalPlayerAbilitySystem(World))
		{
			const FGameplayAttribute Health = UHadesHealthSet::GetHealthAttribute();
			const float Amount = Args.IsEmpty() ? Default : FCString::Atof(*Args[0]);
			AbilitySystem->SetNumericAttributeBase(Health, Change(AbilitySystem->GetNumericAttributeBase(Health), Amount));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs DamageCommand(TEXT("Hades.Health.Damage"), TEXT("Takes health from the local player. Hades.Health.Damage [Amount, 10 by default]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ChangeHealth(Args, World, 10.0f, [](float Health, float Amount) { return Health - Amount; });
		}), ECVF_Cheat);

	FAutoConsoleCommandWithWorldAndArgs HealCommand(TEXT("Hades.Health.Heal"), TEXT("Gives the local player health. Hades.Health.Heal [Amount, 10 by default]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ChangeHealth(Args, World, 10.0f, [](float Health, float Amount) { return Health + Amount; });
		}), ECVF_Cheat);

	FAutoConsoleCommandWithWorldAndArgs SetCommand(TEXT("Hades.Health.Set"), TEXT("Sets the local player's health. Hades.Health.Set [Amount, 100 by default]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ChangeHealth(Args, World, 100.0f, [](float Health, float Amount) { return Amount; });
		}), ECVF_Cheat);
}
#endif
