#pragma once

#include "CoreMinimal.h"

#include "Hades/Character//HadesCharacterBase.h"

#include "GameplayTagContainer.h"
#include "InputActionValue.h"

#include "HadesHeroCharacter.generated.h"

class USceneComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UInputComponent;
class UHadesInputConfig;
struct FHadesAbilityInputAction;
enum class EHadesAbilityInputMode : uint8;

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
	virtual UInputComponent* CreatePlayerInputComponent() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn Interface

	//~ Begin ACharacter Interface
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	//~ End ACharacter Interface

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }

	UFUNCTION(BlueprintPure, Category = "Character|Input")
	FInputActionValue GetInputValue(UPARAM(meta = (Categories = "Input")) FGameplayTag InputTag) const;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USceneComponent> ViewRoot;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Viewmodel")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera", meta = (ClampMin = "0.0", Units = "s"))
	float EyeHeightSmoothingTime = 0.1f;
	
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UnPossessed() override;

protected:
	//~~Start Input
	/** Maps this pawn's Input Actions to input tags. Native tags bind to the handlers below; ability tags drive the ASC. */
	UPROPERTY(EditDefaultsOnly, Category = "Character|Input")
	TObjectPtr<UHadesInputConfig> InputConfig;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_AbilityInputPressed(FGameplayTag InputTag);
	void Input_AbilityInputReleased(FGameplayTag InputTag);

	/** The input's authored mode, or Toggle when it's a Hold input the player has switched to toggle. */
	EHadesAbilityInputMode ResolveAbilityInputMode(const FHadesAbilityInputAction& AbilityInput) const;
	//~~Stop Input

private:
	/** Place ViewRoot EyeHeight above the capsule centre. */
	void SetEyeHeight(float NewEyeHeight);
	
	float EyeHeight = 0.0f;
	float EyeHeightRate = 0.0f;
};