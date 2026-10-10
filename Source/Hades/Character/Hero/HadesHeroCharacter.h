#pragma once

#include "CoreMinimal.h"

#include "Hades/Character//HadesCharacterBase.h"

#include "HadesHeroCharacter.generated.h"

class USceneComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UInputComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class HADES_API AHadesHeroCharacter : public AHadesCharacterBase
{
	GENERATED_BODY()

public:
	AHadesHeroCharacter(const FObjectInitializer& ObjectInitializer  = FObjectInitializer::Get());

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn Interface

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USceneComponent> ViewRoot;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Viewmodel")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

protected:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void RunPressed();
	void RunReleased();
	void CrouchPressed();
	void CrouchReleased();

	//~~Start Input
	UPROPERTY(EditDefaultsOnly, Category = "Character|Input")
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Character|Input")
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, Category = "Character|Input")
	TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Character|Input")
	TObjectPtr<UInputAction> RunAction;
	UPROPERTY(EditDefaultsOnly, Category = "Character|Input")
	TObjectPtr<UInputAction> CrouchAction;
	//~~Stop Input
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Input")
	bool bToggleRun = false;
	/** Player setting. false = hold to crouch, true = press to toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Input")
	bool bToggleCrouch = false;
};
