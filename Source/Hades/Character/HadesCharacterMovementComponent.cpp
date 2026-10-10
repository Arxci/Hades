#include "HadesCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"


void UHadesCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* Char = GetCharacterOwner())
	{
		StandingHalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	CachedVelocity = Velocity;
}

void UHadesCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	UpdateMovementVariables();
	
	UpdateCachedVariables();
}

void UHadesCharacterMovementComponent::StartRunning()
{
	if (bIsRunning) return;

	bIsRunning = true;
	CachedWalkSpeed = MaxWalkSpeed;
	MaxWalkSpeed = MaxWalkSpeedRun;
	OnRunStart.Broadcast();
}

void UHadesCharacterMovementComponent::StopRunning()
{
	if (!bIsRunning) return;

	bIsRunning = false;
	MaxWalkSpeed = CachedWalkSpeed;
	OnRunEnd.Broadcast();
}

bool UHadesCharacterMovementComponent::IsMovingForward(float Threshold) const
{
	if (!IsMovingOnGround()) return false;
	
	const FVector InputVector = GetLastInputVector();
	if (InputVector.IsNearlyZero()) return false;

	const FVector Forward = GetOwner()->GetActorForwardVector();
	return FVector::DotProduct(Forward, InputVector.GetSafeNormal()) > Threshold;
}

bool UHadesCharacterMovementComponent::IsAccelerating() const
{
	const FVector Accel = GetAcceleration();
	
	return FVector::DotProduct(Accel, Velocity) > 0;
}

bool UHadesCharacterMovementComponent::CanStand() const
{
	if (ACharacter* Char = Cast<ACharacter>(GetCharacterOwner()))
	{
		const UCapsuleComponent* Capsule = Char->GetCapsuleComponent();
		const float Radius = Capsule->GetScaledCapsuleRadius();
		const FVector Location = Char->GetActorLocation();

		const float CurrentHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const FVector TraceLocation = Location + FVector(0.f, 0.f, StandingHalfHeight - CurrentHalfHeight);

		TArray<FHitResult> Hits;
		UKismetSystemLibrary::CapsuleTraceMulti(
			Char->GetWorld(),
			TraceLocation,
			TraceLocation,
			Radius,
			StandingHalfHeight,
			UEngineTypes::ConvertToTraceType(ECC_Pawn),
			false,
			{Char},
			EDrawDebugTrace::None,
			Hits,
			true,
			FLinearColor::Green,  
			FLinearColor::Red,    
			0.f  
		);

		return Hits.IsEmpty();
	}
	return true;
}

void UHadesCharacterMovementComponent::UpdateCachedVariables()
{
	CachedVelocity = Velocity;
}

void UHadesCharacterMovementComponent::UpdateMovementVariables()
{
	RelativeAcceleration = CalculateRelativeAcceleration();
	UpdateGroundedAcceleration();
}

void UHadesCharacterMovementComponent::UpdateGroundedAcceleration()
{
	if (AccelerationCurve && IsGrounded())
	{
		const float MappedSpeed = GetMappedSpeed();
		const FVector Value = AccelerationCurve->GetVectorValue(MappedSpeed);

		MaxAcceleration = Value.X;
		BrakingDecelerationWalking = Value.Y;
		GroundFriction = Value.Z;
	}
}


FVector UHadesCharacterMovementComponent::CalculateRelativeAcceleration() const
{
	const FVector Accel = GetAcceleration();

	if (const ACharacter* Char = GetCharacterOwner())
	{
		const bool bAccelerating = FVector::DotProduct(Accel, Velocity) > 0;
		const float MaxMag = bAccelerating ? GetMaxAcceleration() : GetMaxBrakingDeceleration();

		return Char->GetActorRotation().UnrotateVector(
			Accel.GetClampedToMaxSize(MaxMag) / MaxMag);
	}

	return FVector::ZeroVector;
}

FVector UHadesCharacterMovementComponent::GetAcceleration() const
{
	const float DT = GetWorld()->GetDeltaSeconds();
	if (DT < SMALL_NUMBER) return FVector::ZeroVector;

	return (Velocity - CachedVelocity) / DT;
}

float UHadesCharacterMovementComponent::GetMappedSpeed() const
{
	const float Speed = GetSpeed();
	const float WalkSpeed = MaxWalkSpeed;
	const float RunSpeed = MaxWalkSpeedRun;

	if (Speed <= WalkSpeed)
	{
		return FMath::GetMappedRangeValueClamped(
			FVector2D(0.f, WalkSpeed), FVector2D(0.f, 1.f), Speed);
	}

	return FMath::GetMappedRangeValueClamped(
		FVector2D(WalkSpeed, RunSpeed), FVector2D(1.f, 2.f), Speed);
}

float UHadesCharacterMovementComponent::GetSpeed() const
{
	return Velocity.Length();
}