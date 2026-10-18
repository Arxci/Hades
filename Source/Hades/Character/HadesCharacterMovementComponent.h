#pragma once

#include "CoreMinimal.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "HadesCharacterMovementComponent.generated.h"

class UCurveVector;
class UHadesMovementSet;


UCLASS()
class HADES_API UHadesCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//~ Begin UCharacterMovementComponent Interface
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual float GetMaxSpeed() const override;
	//~ End UCharacterMovementComponent Interface

public:
	// Movement
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Run")
	void SetWantsToRun(bool bNewWantsToRun) { bWantsToRun = bNewWantsToRun; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Run")
	float MaxWalkSpeedRun = 1000.0f;

	/** Sampled by mapped speed (0 = still, 1 = walk, 2 = run) while grounded. X = MaxAcceleration, Y = BrakingDecelerationWalking, Z = GroundFriction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Movement: Walking")
	TObjectPtr<UCurveVector> AccelerationCurve;

public:
	// Utility
	UFUNCTION(BlueprintPure, Category = "Character Movement: Run")
	bool IsRunning() const { return bIsRunning; }
	UFUNCTION(BlueprintPure, Category = "Character Movement: Crouch")
	bool CanStand() const;
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	bool IsMovingForward(float Threshold = 0.2f) const;
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	bool IsAccelerating() const;
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	float GetSpeed() const;
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	float GetMappedSpeed() const;
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	FVector GetRelativeAcceleration() const { return RelativeAcceleration; }
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	FVector GetAcceleration() const { return MeasuredAcceleration; }

private:
	void StartRunning();
	void StopRunning();

	float GetMoveSpeedMultiplier() const;

	UPROPERTY(Transient)
	TObjectPtr<const UHadesMovementSet> MovementSet;

	bool bWantsToRun = false;
	bool bIsRunning = false;

	FVector LastVelocity = FVector::ZeroVector;
	FVector MeasuredAcceleration = FVector::ZeroVector;
	FVector RelativeAcceleration = FVector::ZeroVector;
};