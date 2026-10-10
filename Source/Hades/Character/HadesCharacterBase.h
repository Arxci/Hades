#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Character.h"

#include "Hades/AbilitySystem/HadesAbilitySystemInterface.h"

#include "HadesCharacterBase.generated.h"

class UHadesAbilitySystemComponent;
class UHadesAbility;

class UHadesCharacterMovementComponent;

UCLASS(PrioritizeCategories = ("Abilities"))
class HADES_API AHadesCharacterBase : public ACharacter, public IHadesAbilitySystemInterface
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
	//Movement
	UFUNCTION()
	void OnStartRun();
	UFUNCTION()
	void OnEndRun();
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
	virtual UHadesAbilitySystemComponent* GetHadesAbilitySystemComponent() const override { return HadesAbilitySystemComponent; };
	UFUNCTION(BlueprintPure, Category = "Character")
	UHadesCharacterMovementComponent* GetHadesCharacterMovement() const { return HadesCharacterMovement; };

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TArray<TSubclassOf<UHadesAbility>> DefaultAbilities;
};
