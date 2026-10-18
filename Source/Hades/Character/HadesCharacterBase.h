#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Character.h"

#include "AbilitySystemInterface.h"

#include "HadesCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UHadesAbilitySystemComponent;
class UHadesMovementSet;

class UHadesCharacterMovementComponent;

UCLASS(PrioritizeCategories = ("Abilities"))
class HADES_API AHadesCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AHadesCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UHadesAbilitySystemComponent> HadesAbilitySystemComponent;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UHadesCharacterMovementComponent> HadesCharacterMovement;

public:
	//~ Begin IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	//~ Begin ACharacter Interface
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	//~ End ACharacter Interface

public:
	//Movement
	virtual void OnStartRun();
	virtual void OnEndRun();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Character", meta = (DisplayName = "OnStartRun", ScriptName = "OnStartRun"))
	void K2_OnStartRun();
	UFUNCTION(BlueprintImplementableEvent, Category = "Character", meta = (DisplayName = "OnEndRun", ScriptName = "OnEndRun"))
	void K2_OnEndRun();
	
	UFUNCTION(BlueprintCallable, Category = "Character")
	void Run() const;
	UFUNCTION(BlueprintCallable, Category = "Character")
	void StopRun() const;

public:
	//Utility
	UFUNCTION(BlueprintPure)
	bool GetIsCrouched() const;
	UFUNCTION(BlueprintPure)
	bool GetIsRunning() const;
	UFUNCTION(BlueprintPure)
	bool GetIsGrounded() const;
	UHadesAbilitySystemComponent* GetHadesAbilitySystemComponent() const { return HadesAbilitySystemComponent; };
	UFUNCTION(BlueprintPure, Category = "Character")
	UHadesCharacterMovementComponent* GetHadesCharacterMovement() const { return HadesCharacterMovement; };

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Applied once at BeginPlay. Use an Instant effect that overrides attributes to set this character's starting values. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TArray<TSubclassOf<UGameplayEffect>> DefaultEffects;

private:
	/** Created as a subobject so the ability system component finds and registers it on its own. */
	UPROPERTY()
	TObjectPtr<UHadesMovementSet> MovementSet;
};
