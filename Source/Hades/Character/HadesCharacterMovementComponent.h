#pragma once

#include "CoreMinimal.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "Curves/CurveVector.h"

#include "HadesCharacterMovementComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRunStateChanged);


UCLASS()
class HADES_API UHadesCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// Movement
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Run")
	void StartRunning();
	UFUNCTION(BlueprintCallable, Category = "Character Movement: Run")
	void StopRunning();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Movement: Run")
	float MaxWalkSpeedRun = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	TObjectPtr<UCurveVector> AccelerationCurve;

public:
	// Utility
	UFUNCTION(BlueprintPure, Category = "Character Movement: Run")
	bool IsRunning() const { return bIsRunning; }
	UFUNCTION(BlueprintPure, Category = "Character Movement: Crouch")
	bool CanStand() const;
	UFUNCTION(BlueprintPure, Category = "Character Movement")
	bool IsGrounded() const { return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking; }
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
	FVector GetAcceleration() const;

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Character Movement: Run")
	FOnRunStateChanged OnRunStart;

	UPROPERTY(BlueprintAssignable, Category = "Character Movement: Run")
	FOnRunStateChanged OnRunEnd;

private:
	bool bIsRunning = false;
	float StandingHalfHeight = 0.0f;
	float CachedWalkSpeed = 0.0f;

	void UpdateMovementVariables();
	void UpdateGroundedAcceleration();
	void UpdateCachedVariables();
	
	FVector RelativeAcceleration;
	FVector CachedVelocity;
	
	FVector CalculateRelativeAcceleration() const;
};