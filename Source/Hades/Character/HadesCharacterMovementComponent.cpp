#include "HadesCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/World.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

#include "Hades/AbilitySystem/Attributes/HadesMovementSet.h"
#include "Hades/Character/HadesCharacterBase.h"


void UHadesCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		MovementSet = ASC->GetSet<UHadesMovementSet>();
	}

	LastVelocity = Velocity;
}

void UHadesCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime > UE_SMALL_NUMBER)
	{
		MeasuredAcceleration = (Velocity - LastVelocity) / DeltaTime;
	}
	LastVelocity = Velocity;

	const float MaxAccel = IsAccelerating() ? GetMaxAcceleration() : GetMaxBrakingDeceleration();
	RelativeAcceleration = MaxAccel > UE_SMALL_NUMBER
		? GetOwner()->GetActorQuat().UnrotateVector(MeasuredAcceleration.GetClampedToMaxSize(MaxAccel) / MaxAccel)
		: FVector::ZeroVector;
}

void UHadesCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	if (bWantsToRun && !IsCrouching() && IsMovingForward())
	{
		StartRunning();
	}
	else
	{
		StopRunning();
	}

	if (AccelerationCurve && IsMovingOnGround())
	{
		const FVector Value = AccelerationCurve->GetVectorValue(GetMappedSpeed());
		MaxAcceleration = Value.X;
		BrakingDecelerationWalking = Value.Y;
		GroundFriction = Value.Z;
	}
}

bool UHadesCharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && (IsMovingOnGround() || IsCrouching());
}

float UHadesCharacterMovementComponent::GetMaxSpeed() const
{
	const float BaseSpeed = (IsRunning() && IsMovingOnGround()) ? MaxWalkSpeedRun : Super::GetMaxSpeed();
	return BaseSpeed * GetMoveSpeedMultiplier();
}

float UHadesCharacterMovementComponent::GetMoveSpeedMultiplier() const
{
	return MovementSet ? MovementSet->GetMoveSpeedMultiplier() : 1.0f;
}

void UHadesCharacterMovementComponent::StartRunning()
{
	if (bIsRunning) return;

	bIsRunning = true;
	
	if (AHadesCharacterBase* HadesOwner = Cast<AHadesCharacterBase>(GetCharacterOwner()))
	{
		HadesOwner->OnStartRun();
	}
}

void UHadesCharacterMovementComponent::StopRunning()
{
	if (!bIsRunning) return;

	bIsRunning = false;

	if (AHadesCharacterBase* HadesOwner = Cast<AHadesCharacterBase>(GetCharacterOwner()))
	{
		HadesOwner->OnEndRun();
	}
}

bool UHadesCharacterMovementComponent::IsMovingForward(float Threshold) const
{
	// Acceleration (not the pawn's input vector) is what the server receives and what replays use.
	if (!IsMovingOnGround() || Acceleration.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(GetOwner()->GetActorForwardVector(), Acceleration.GetSafeNormal()) > Threshold;
}

bool UHadesCharacterMovementComponent::IsAccelerating() const
{
	return FVector::DotProduct(MeasuredAcceleration, Velocity) > 0.0f;
}

bool UHadesCharacterMovementComponent::CanStand() const
{
	const ACharacter* Character = GetCharacterOwner();
	if (!Character || !IsCrouching())
	{
		return true;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float StandingHalfHeight = Character->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Capsule->GetShapeScale();
	const FVector StandingLocation = Capsule->GetComponentLocation() + FVector(0.0f, 0.0f, StandingHalfHeight - Capsule->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CanStand), false, Character);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(QueryParams, ResponseParams);

	return !GetWorld()->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), StandingHalfHeight), QueryParams, ResponseParams);
}

float UHadesCharacterMovementComponent::GetMappedSpeed() const
{
	const float Speed = GetSpeed();
	const float WalkSpeed = MaxWalkSpeed * GetMoveSpeedMultiplier();
	const float RunSpeed = MaxWalkSpeedRun * GetMoveSpeedMultiplier();

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