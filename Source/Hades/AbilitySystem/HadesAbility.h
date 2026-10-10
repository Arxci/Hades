#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "HadesAbility.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActivationStateChanged, UHadesAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnabledStateChanged);


UENUM(BlueprintType)
enum class EHadesAbilityActivationState : uint8
{
	Idle    UMETA(DisplayName = "Idle"),
	Active  UMETA(DisplayName = "Active"),
};

UCLASS(Blueprintable)
class HADES_API UHadesAbility : public UObject
{
	GENERATED_BODY()

	friend class UHadesAbilitySystemComponent;

public:
	UHadesAbility();

	//Ability life cycle
	virtual bool RequestActivateAbility();
	virtual bool RequestDeactivateAbility(bool bForce=false);
	

	virtual void ForceEndAbility();
	virtual void OnEnableAbility();
	virtual void OnDisableAbility();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Hades Ability|Lifecycle")
	bool CanActivateAbility() const;
	virtual bool CanActivateAbility_Implementation() const;

public:
	// Utility
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Util")
	UHadesAbilitySystemComponent* GetHadesAbilitySystemComponent() const;
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Util")
    AActor* GetOwner() const;
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Util")
	AController* GetController() const;

public:
	// State
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|State")
	EHadesAbilityActivationState GetActivationState() const { return ActivationState; }
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|State")
	bool IsActive() const { return ActivationState == EHadesAbilityActivationState::Active; }
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|State")
	bool IsIdle() const { return ActivationState == EHadesAbilityActivationState::Idle; }
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|State")
	bool IsEnabled() const { return bIsEnabled; }

public:
	// Cooldown
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Cooldown")
	bool IsOnCooldown() const { return bIsOnCooldown; }
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Cooldown")
	virtual bool HasCooldown() const;
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Cooldown")
	virtual float GetCooldownTotalDuration() const;
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Cooldown")
	float GetCooldownTimeRemaining() const;
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Cooldown")
	float GetCooldownProgress() const;

public:
	// Delegates
	UPROPERTY(BlueprintAssignable)
	FOnActivationStateChanged OnActivated;
	UPROPERTY(BlueprintAssignable)
	FOnActivationStateChanged OnDeactivated;
	UPROPERTY(BlueprintAssignable)
	FOnEnabledStateChanged OnAbilityDisabled;
	UPROPERTY(BlueprintAssignable)
	FOnEnabledStateChanged OnAbilityEnabled;
	
protected:
	virtual void TickAbility(float DeltaTime) { K2_OnTickAbility(); }
	virtual bool CanTick() { return false; }
	virtual void InitializeAbility() { K2_OnInitializeAbility(); };
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability",
		meta = (DisplayName = "On Tick Ability", ScriptName = "OnTickAbility"))
	void K2_OnTickAbility();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability",
	meta = (DisplayName = "On Initialize Ability", ScriptName = "OnInitializeAbility"))
	void K2_OnInitializeAbility();

	//Ability life cycle
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Lifecycle")
	virtual void CommitAbility();
	UFUNCTION(BlueprintCallable, Category = "Hades Ability|Lifecycle")
	virtual void CommitAbilityEnd();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability|Lifecycle",
	meta = (DisplayName = "On Ability Committed", ScriptName = "OnAbilityCommitted"))
	void K2_OnAbilityCommitted();
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability|Lifecycle",
		meta = (DisplayName = "On Ability Ended", ScriptName = "OnAbilityEnded"))
	void K2_OnAbilityEnded();
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability|Lifecycle",
	meta = (DisplayName = "On Ability Activate", ScriptName = "OnAbilityActivate"))
	void K2_OnAbilityActivate();
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability|Lifecycle",
		meta = (DisplayName = "On Ability Deactivate", ScriptName = "OnAbilityDeactivate"))
	void K2_OnAbilityDeactivate();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability|Lifecycle",
		meta = (DisplayName = "On Ability Enabled", ScriptName = "OnActivateEnabled"))
	void K2_OnAbilityEnabled();
	UFUNCTION(BlueprintImplementableEvent, Category = "Hades Ability|Lifecycle",
		meta = (DisplayName = "On Ability Disabled", ScriptName = "OnDeactivateDisabled"))
	void K2_OnAbilityDisabled();

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Ability System|Abilities")
	bool bIsEnabled = true;

	UPROPERTY(SaveGame)
	EHadesAbilityActivationState ActivationState = EHadesAbilityActivationState::Idle;

	
	//Tags
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Abilities")
	FGameplayTagContainer AbilityTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Abilities")
	FGameplayTagContainer ActivationOwnedTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Abilities")
	FGameplayTagContainer ActivationRequiredTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Abilities")
	FGameplayTagContainer ActivationBlockedTags;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Abilities")
	FGameplayTagContainer CancelAbilitiesWithTags;
	

	//Cooldown
	void RestartCooldown();
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Ability System|Cooldown", meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Cooldown")
	bool bCooldownOnActivation = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability System|Cooldown")
	bool bCooldownOnDeactivation = true;

	virtual class UWorld* GetWorld() const override;

private:
	
	UPROPERTY(SaveGame)
	bool bIsOnCooldown = false;
	UPROPERTY(SaveGame)
	float CooldownElapsed = 0.0f;

	void StartCooldown();
	void TickCooldown(float DeltaTime);
};
