#include "HadesCameraLagComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

#include "Hades/Hades.h"

namespace HadesCameraLag
{
	static TAutoConsoleVariable<bool> CVarEnable(
		TEXT("Hades.CameraLag.Enable"),
		true,
		TEXT("Turn camera lag off to compare the view with and without it."));

	static TAutoConsoleVariable<bool> CVarDebug(
		TEXT("Hades.CameraLag.Debug"),
		false,
		TEXT("Prints how far the view trails the aim and the eye. Also draws both points, which the debug camera shows."));

	/** A jump further than this in one frame is a teleport, so the view cuts to it instead of easing. */
	constexpr double TeleportDistance = 100.0;

	/** About the near clip plane. The view stays this far inside the capsule, so it can't see into walls or ceilings. */
	constexpr double CapsuleClearance = 10.0;

	/**
	 * Advances a critically damped spring that pulls a value toward a moving target. Offset is the value minus the target and Rate
	 * is the value's rate of change; the target moves by Target Move, evenly over Delta Time. The step is exact, so the motion is the
	 * same at any frame rate, and a target moving steadily is followed Smoothing Time behind. A Smoothing Time of 0 snaps.
	 */
	static void StepSpring(double& Offset, double& Rate, double TargetMove, double DeltaTime, double SmoothingTime)
	{
		if (DeltaTime <= 0.0)
		{
			Offset -= TargetMove;
			return;
		}

		const double TargetRate = TargetMove / DeltaTime;
		if (SmoothingTime <= UE_KINDA_SMALL_NUMBER)
		{
			Offset = 0.0;
			Rate = TargetRate;
			return;
		}

		// Measured from where steady following settles, what's left decays like a spring nothing pushes.
		const double Omega = 2.0 / SmoothingTime;
		const double SteadyOffset = -TargetRate * SmoothingTime;
		const double Decay = FMath::Exp(-Omega * DeltaTime);
		double Settle = Offset - SteadyOffset;
		double SettleRate = Rate - TargetRate;
		const double Push = (SettleRate + Omega * Settle) * DeltaTime;
		Settle = (Settle + Push) * Decay;
		SettleRate = (SettleRate - Omega * Push) * Decay;

		Offset = SteadyOffset + Settle;
		Rate = TargetRate + SettleRate;
	}
}

const FName UHadesCameraLagComponent::SocketName(TEXT("View"));

UHadesCameraLagComponent::UHadesCameraLagComponent()
{
	// After character movement and the owner's own tick, either of which can move the eye.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UHadesCameraLagComponent::OnRegister()
{
	Super::OnRegister();

	ResetLag();
}

void UHadesCameraLagComponent::BeginPlay()
{
	Super::BeginPlay();

	// A camera that turns itself to the controller undoes the rotation lag every frame.
	if (RotationLagTime > 0.0f)
	{
		for (const USceneComponent* Child : GetAttachChildren())
		{
			const UCameraComponent* Camera = Cast<UCameraComponent>(Child);
			if (Camera && Camera->bUsePawnControlRotation)
			{
				UE_LOG(LogHadesCamera, Warning, TEXT("[Camera] %s has Use Pawn Control Rotation on, which skips %s's rotation lag. Turn it off; %s turns the view to the aim."),
					*GetNameSafe(Camera), *GetName(), *GetName());
			}
		}
	}
}

void UHadesCameraLagComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateLag(DeltaTime);
}

void UHadesCameraLagComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	PreviousTargetLocation += InOffset;
}

bool UHadesCameraLagComponent::HasAnySockets() const
{
	return true;
}

void UHadesCameraLagComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	OutSockets.Emplace(SocketName, EComponentSocketType::Socket);
}

FTransform UHadesCameraLagComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	// Every socket, and none, is the lagged view, so whatever is attached follows it.
	const FTransform RelativeTransform(RelativeSocketRotation, RelativeSocketLocation);
	switch (TransformSpace)
	{
	case RTS_World:
		return RelativeTransform * GetComponentTransform();
	case RTS_Actor:
		if (const AActor* Actor = GetOwner())
		{
			return (RelativeTransform * GetComponentTransform()).GetRelativeTransform(Actor->GetTransform());
		}
		break;
	case RTS_Component:
	case RTS_ParentBoneSpace:
	default:
		break;
	}
	return RelativeTransform;
}

void UHadesCameraLagComponent::ResetLag()
{
	PreviousTargetLocation = GetComponentLocation();
	PreviousTargetVelocity = GetTargetVelocity();
	PreviousTargetRotation = GetTargetRotation();

	LocationOffset = FVector::ZeroVector;
	LocationOffsetRate = FVector::ZeroVector;
	ViewRotation = PreviousTargetRotation;
	ViewRotationRate = FRotator::ZeroRotator;

	UpdateSocketTransform(ETeleportType::TeleportPhysics);
}

void UHadesCameraLagComponent::UpdateLag(float DeltaTime)
{
	// A new controller can aim anywhere, so start from its aim instead of swinging round to it.
	const APawn* Pawn = GetOwner<APawn>();
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (Controller && Controller != LastController.Get())
	{
		LastController = Controller;
		ResetLag();
		return;
	}

	const bool bEnabled = HadesCameraLag::CVarEnable.GetValueOnGameThread();
	const float HorizontalTime = bEnabled ? HorizontalLagTime : 0.0f;
	const float VerticalTime = bEnabled ? VerticalLagTime : 0.0f;
	const float RotationTime = bEnabled ? RotationLagTime : 0.0f;

	const FVector TargetLocation = GetComponentLocation();
	const FVector TargetVelocity = GetTargetVelocity();
	const FRotator TargetRotation = GetTargetRotation();

	// Location. The offset is kept in a frame that moves with the eye's own velocity, so moving steadily leaves it alone. When the
	// velocity changes, the view carries on as it was moving and the spring settles it. Character movement changes velocity
	// partway through a frame on landings and walls, so find when, or how much the view carries on would depend on frame rate.
	// Whatever the velocity doesn't explain, such as a step, the view eases after.
	const FVector Moved = TargetLocation - PreviousTargetLocation;
	const FVector VelocityChange = TargetVelocity - PreviousTargetVelocity;
	double SwitchTime = 0.0;
	if (DeltaTime > 0.0f && VelocityChange.SizeSquared() > UE_KINDA_SMALL_NUMBER)
	{
		// Moved = PreviousTargetVelocity * SwitchTime + TargetVelocity * (DeltaTime - SwitchTime), solved for SwitchTime.
		SwitchTime = FMath::Clamp(FVector::DotProduct(Moved - TargetVelocity * DeltaTime, -VelocityChange) / VelocityChange.SizeSquared(),
			0.0, static_cast<double>(DeltaTime));
	}
	const FVector Unexplained = Moved - PreviousTargetVelocity * SwitchTime - TargetVelocity * (DeltaTime - SwitchTime);

	if (Unexplained.SizeSquared() > FMath::Square(HadesCameraLag::TeleportDistance))
	{
		LocationOffset = FVector::ZeroVector;
		LocationOffsetRate = FVector::ZeroVector;
	}
	else
	{
		const double BeforeSwitch = DeltaTime > 0.0f ? SwitchTime / DeltaTime : 0.0;
		StepLocationLag(SwitchTime, Unexplained * BeforeSwitch, HorizontalTime, VerticalTime);
		LocationOffsetRate -= VelocityChange;
		StepLocationLag(DeltaTime - SwitchTime, Unexplained * (1.0 - BeforeSwitch), HorizontalTime, VerticalTime);
		ClampLocationLag(TargetLocation);
	}
	PreviousTargetLocation = TargetLocation;
	PreviousTargetVelocity = TargetVelocity;

	// Rotation. Nothing is fed forward, so turning trails by Rotation Lag Time. The aim is taken to sweep evenly through the frame,
	// which keeps that true at any frame rate.
	const FRotator AimMove = (TargetRotation - PreviousTargetRotation).GetNormalized();
	FRotator RotationOffset = (ViewRotation - PreviousTargetRotation).GetNormalized();
	HadesCameraLag::StepSpring(RotationOffset.Pitch, ViewRotationRate.Pitch, AimMove.Pitch, DeltaTime, RotationTime);
	HadesCameraLag::StepSpring(RotationOffset.Yaw, ViewRotationRate.Yaw, AimMove.Yaw, DeltaTime, RotationTime);
	HadesCameraLag::StepSpring(RotationOffset.Roll, ViewRotationRate.Roll, AimMove.Roll, DeltaTime, RotationTime);
	ViewRotation = (TargetRotation + RotationOffset).GetNormalized();
	PreviousTargetRotation = TargetRotation;

	UpdateSocketTransform(ETeleportType::None);

#if ENABLE_DRAW_DEBUG
	if (HadesCameraLag::CVarDebug.GetValueOnGameThread())
	{
		const FVector ViewLocation = TargetLocation + LocationOffset;
		if (const UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, TargetLocation, 2.0f, 8, FColor::Green);
			DrawDebugSphere(World, ViewLocation, 2.0f, 8, FColor::Yellow);
			DrawDebugLine(World, TargetLocation, ViewLocation, FColor::Yellow);
		}

		if (GEngine)
		{
			const double AimLag = FMath::RadiansToDegrees(ViewRotation.Quaternion().AngularDistance(TargetRotation.Quaternion()));
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow,
				FString::Printf(TEXT("Camera lag %.1f deg behind the aim, %.1f cm off the eye (%.1f across, %.1f up)"),
					AimLag, LocationOffset.Size(), LocationOffset.Size2D(), LocationOffset.Z));
		}
	}
#endif
}

void UHadesCameraLagComponent::StepLocationLag(double DeltaTime, const FVector& UnexplainedMove, float HorizontalTime, float VerticalTime)
{
	HadesCameraLag::StepSpring(LocationOffset.X, LocationOffsetRate.X, UnexplainedMove.X, DeltaTime, HorizontalTime);
	HadesCameraLag::StepSpring(LocationOffset.Y, LocationOffsetRate.Y, UnexplainedMove.Y, DeltaTime, HorizontalTime);
	HadesCameraLag::StepSpring(LocationOffset.Z, LocationOffsetRate.Z, UnexplainedMove.Z, DeltaTime, VerticalTime);
}

void UHadesCameraLagComponent::ClampLocationLag(const FVector& TargetLocation)
{
	double MaxAcross = MaxHorizontalLag;
	double MaxRise = MaxVerticalLag;

	// The capsule is the one space known to be clear.
	const ACharacter* Character = GetOwner<ACharacter>();
	if (const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr)
	{
		const double CapsuleTop = Capsule->GetComponentLocation().Z + Capsule->GetScaledCapsuleHalfHeight();
		MaxAcross = FMath::Min(MaxAcross, Capsule->GetScaledCapsuleRadius() - HadesCameraLag::CapsuleClearance);
		MaxRise = FMath::Min(MaxRise, CapsuleTop - HadesCameraLag::CapsuleClearance - TargetLocation.Z);
	}
	MaxAcross = FMath::Max(MaxAcross, 0.0);
	MaxRise = FMath::Max(MaxRise, 0.0);

	const double Across = LocationOffset.Size2D();
	if (Across > MaxAcross)
	{
		const FVector Direction(LocationOffset.X / Across, LocationOffset.Y / Across, 0.0);
		LocationOffset.X = Direction.X * MaxAcross;
		LocationOffset.Y = Direction.Y * MaxAcross;

		// Stop drifting outward, or the view would stay pinned to the limit.
		const double Outward = FVector::DotProduct(LocationOffsetRate, Direction);
		if (Outward > 0.0)
		{
			LocationOffsetRate -= Direction * Outward;
		}
	}

	if (LocationOffset.Z > MaxRise)
	{
		LocationOffset.Z = MaxRise;
		LocationOffsetRate.Z = FMath::Min(LocationOffsetRate.Z, 0.0);
	}
	else if (LocationOffset.Z < -MaxVerticalLag)
	{
		LocationOffset.Z = -MaxVerticalLag;
		LocationOffsetRate.Z = FMath::Max(LocationOffsetRate.Z, 0.0);
	}
}

void UHadesCameraLagComponent::UpdateSocketTransform(ETeleportType Teleport)
{
	const FTransform ViewTransform(ViewRotation, GetComponentLocation() + LocationOffset);
	const FTransform RelativeView = ViewTransform.GetRelativeTransform(GetComponentTransform());
	RelativeSocketLocation = RelativeView.GetLocation();
	RelativeSocketRotation = RelativeView.GetRotation();

	UpdateChildTransforms(EUpdateTransformFlags::None, Teleport);
}

FRotator UHadesCameraLagComponent::GetTargetRotation() const
{
	const APawn* Pawn = GetOwner<APawn>();
	return bUsePawnControlRotation && Pawn ? Pawn->GetViewRotation() : GetComponentRotation();
}

FVector UHadesCameraLagComponent::GetTargetVelocity() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	FVector Velocity = Owner->GetVelocity();

	// Walking keeps velocity level, yet character movement carries the capsule up and down ramps along the floor. Follow the floor
	// the same way, so a ramp doesn't leave the view behind. Stair treads are level, so steps still count as jumps to ease after.
	const ACharacter* Character = Cast<ACharacter>(Owner);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor() && !Movement->CurrentFloor.bLineTrace)
	{
		const FHitResult& Floor = Movement->CurrentFloor.HitResult;
		if (Floor.ImpactNormal.Z > UE_KINDA_SMALL_NUMBER && Floor.ImpactNormal.Z < 1.0 - UE_KINDA_SMALL_NUMBER && Floor.Normal.Z > UE_KINDA_SMALL_NUMBER)
		{
			const FVector Level(Velocity.X, Velocity.Y, 0.0);
			const FVector AlongFloor(Level.X, Level.Y, -FVector::DotProduct(Floor.ImpactNormal, Level) / Floor.ImpactNormal.Z);
			Velocity = Movement->bMaintainHorizontalGroundVelocity ? AlongFloor : AlongFloor.GetSafeNormal() * Level.Size();
		}
	}

	return Velocity;
}
