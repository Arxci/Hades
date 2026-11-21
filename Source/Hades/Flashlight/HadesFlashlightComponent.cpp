#include "HadesFlashlightComponent.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "Hades/Hades.h"

namespace
{
	const FName BeamShapeParameter(TEXT("BeamShape"));
	const FName BeamColorParameter(TEXT("BeamColor"));
	const FName BeamNoiseParameter(TEXT("BeamNoise"));
	const FName BeamLookParameter(TEXT("BeamLook"));

	// A critically damped spring toward Target, the same response FMath::CriticallyDampedSmoothing gives a float.
	void CriticallyDampedStep(FVector& Value, FVector& Rate, const FVector& Target, float DeltaTime, float SmoothingTime)
	{
		if (SmoothingTime <= UE_KINDA_SMALL_NUMBER)
		{
			Value = Target;
			Rate = FVector::ZeroVector;
			return;
		}

		const double Omega = 2.0 / SmoothingTime;
		const double Decay = FMath::Exp(-Omega * DeltaTime);
		const FVector Change = Value - Target;
		const FVector Temp = (Rate + Change * Omega) * DeltaTime;
		Rate = (Rate - Temp * Omega) * Decay;
		Value = Target + (Change + Temp) * Decay;
	}

	// Keeps Aim within MaxDegrees of Target, dropping the part of its motion that would carry it further away.
	void LimitLag(FVector& Aim, FVector& Rate, const FVector& Target, float MaxDegrees)
	{
		if (FVector::DotProduct(Aim, Target) >= FMath::Cos(FMath::DegreesToRadians(MaxDegrees)))
		{
			return;
		}

		const FVector Axis = FVector::CrossProduct(Target, Aim).GetSafeNormal();
		if (Axis.IsZero())
		{
			Aim = Target;
			Rate = FVector::ZeroVector;
			return;
		}

		Aim = Target.RotateAngleAxis(MaxDegrees, Axis);
		const FVector Away = (Aim - Target).GetSafeNormal();
		const double SpeedAway = FVector::DotProduct(Rate, Away);
		if (SpeedAway > 0.0)
		{
			Rate -= Away * SpeedAway;
		}
	}

	// Aim as it should be shown: exactly where it is up to 70% of MaxDegrees from Target, then easing toward MaxDegrees
	// without a corner, so it never stops dead.
	FVector EaseLag(const FVector& Aim, const FVector& Target, float MaxDegrees)
	{
		if (MaxDegrees <= 0.0f)
		{
			return Target;
		}

		const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Aim, Target), -1.0, 1.0)));
		const double Knee = 0.7 * MaxDegrees;
		if (Angle <= Knee)
		{
			return Aim;
		}

		const FVector Axis = FVector::CrossProduct(Target, Aim).GetSafeNormal();
		if (Axis.IsZero())
		{
			return Target;
		}

		// tanh, which starts with the same slope as the spring's own lag at the knee.
		const double Room = MaxDegrees - Knee;
		const double Over = (Angle - Knee) / Room;
		const double Eased = Knee + Room * (1.0 - 2.0 / (FMath::Exp(2.0 * Over) + 1.0));
		return Target.RotateAngleAxis(Eased, Axis);
	}
}

UHadesFlashlightComponent::UHadesFlashlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After the pawn has moved and turned for the frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	BeamProxyMesh = CubeMesh.Object;
}

void UHadesFlashlightComponent::BeginPlay()
{
	Super::BeginPlay();

	for (USceneComponent* Child : GetAttachChildren())
	{
		if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Child))
		{
			Light = SpotLight;
			break;
		}
	}
	if (!Light)
	{
		UE_LOG(LogHadesFlashlight, Warning, TEXT("[Flashlight] %s has no spot light attached, so it has nothing to aim."), *GetPathNameSafe(this));
	}

	if (const AActor* Owner = GetOwner())
	{
		ViewCamera = Owner->FindComponentByClass<UCameraComponent>();
	}

	CreateBeam();

	bFlashlightOn = bStartOn;
	ApplyOnState();
}

void UHadesFlashlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyBeam();

	Super::EndPlay(EndPlayReason);
}

void UHadesFlashlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePlacement(DeltaTime, false);
	UpdateBeam();
}

void UHadesFlashlightComponent::SetFlashlightOn(bool bOn)
{
	if (bFlashlightOn == bOn)
	{
		return;
	}

	bFlashlightOn = bOn;
	ApplyOnState();
}

void UHadesFlashlightComponent::ApplyOnState()
{
	if (Light)
	{
		Light->SetVisibility(bFlashlightOn);
	}
	if (BeamComponent)
	{
		BeamComponent->SetVisibility(bFlashlightOn);
	}

	SetComponentTickEnabled(bFlashlightOn);

	if (bFlashlightOn)
	{
		// Tick won't run until next frame, and the light should come on pointing where the view is, not where it was when it went off.
		UpdatePlacement(0.0f, true);
		UpdateBeam();
	}
}

void UHadesFlashlightComponent::SetBeamMaterial(UMaterialInterface* NewBeamMaterial)
{
	if (BeamMaterial == NewBeamMaterial)
	{
		return;
	}

	BeamMaterial = NewBeamMaterial;

	if (HasBegunPlay())
	{
		DestroyBeam();
		CreateBeam();
		if (BeamComponent)
		{
			BeamComponent->SetVisibility(bFlashlightOn);
			UpdateBeam();
		}
	}
}

void UHadesFlashlightComponent::SnapToView()
{
	UpdatePlacement(0.0f, true);
}

bool UHadesFlashlightComponent::GetViewPoint(FVector& OutLocation, FQuat& OutRotation) const
{
	// Not the controller's view point: the camera manager only updates that at the end of the frame, so it's a frame
	// behind here and the light would trail the camera when moving. The camera's location already follows the pawn,
	// and the pawn's view rotation is this frame's.
	const USceneComponent* ViewComponent = ViewCamera.IsValid() ? ViewCamera.Get() : GetAttachParent();
	if (!ViewComponent)
	{
		return false;
	}

	OutLocation = ViewComponent->GetComponentLocation();

	const APawn* Pawn = GetOwner<APawn>();
	OutRotation = Pawn ? Pawn->GetViewRotation().Quaternion() : ViewComponent->GetComponentQuat();
	return true;
}

void UHadesFlashlightComponent::UpdatePlacement(float DeltaTime, bool bSnap)
{
	FVector ViewLocation;
	FQuat ViewRotation;
	const UWorld* World = GetWorld();
	if (!World || !GetViewPoint(ViewLocation, ViewRotation))
	{
		return;
	}

	const FVector ViewForward = ViewRotation.GetForwardVector();
	const FVector ViewUp = ViewRotation.GetUpVector();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HadesFlashlight), false, GetOwner());
	if (const AActor* Owner = GetOwner())
	{
		TArray<AActor*> AttachedActors;
		Owner->GetAttachedActors(AttachedActors);
		QueryParams.AddIgnoredActors(AttachedActors);
	}

	// Where the hand holds it, kept in front of the body, and pulled back toward the eye if that would put it in a wall.
	FVector LightLocation = ViewLocation + ViewRotation.RotateVector(HandOffset);
	if (BodyClearance > 0.0f)
	{
		const FVector LevelForward = FRotator(0.0, ViewRotation.Rotator().Yaw, 0.0).Vector();
		const double InFront = FVector::DotProduct(LightLocation - ViewLocation, LevelForward);
		if (InFront < BodyClearance)
		{
			LightLocation += LevelForward * (BodyClearance - InFront);
		}
	}
	if (WallClearance > 0.0f)
	{
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, ViewLocation, LightLocation, FQuat::Identity, TraceChannel,
			FCollisionShape::MakeSphere(WallClearance), QueryParams))
		{
			LightLocation = Hit.Location;
		}
	}

	// Point at what's under the crosshair, at a smoothed distance so the aim doesn't jump between near and far surfaces.
	FVector TargetDirection = ViewForward;
	if (bAimAtCrosshair)
	{
		const float AimRange = FMath::Max(MinAimDistance, MaxAimDistance);
		float TargetDistance = AimRange;

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewForward * AimRange, TraceChannel, QueryParams))
		{
			TargetDistance = Hit.Distance;
		}
		TargetDistance = FMath::Clamp(TargetDistance, MinAimDistance, AimRange);

		if (bSnap || AimDistance <= 0.0f)
		{
			AimDistance = TargetDistance;
			AimDistanceRate = 0.0f;
		}
		else
		{
			FMath::CriticallyDampedSmoothing(AimDistance, AimDistanceRate, TargetDistance, 0.0f, DeltaTime, AimSmoothingTime);
		}

		const FVector AimPoint = ViewLocation + ViewForward * AimDistance;
		TargetDirection = (AimPoint - LightLocation).GetSafeNormal(UE_SMALL_NUMBER, ViewForward);
	}

	// Trail behind the view on a spring.
	if (bSnap || LagSmoothingTime <= 0.0f)
	{
		AimDirection = TargetDirection;
		AimDirectionRate = FVector::ZeroVector;
	}
	else
	{
		CriticallyDampedStep(AimDirection, AimDirectionRate, TargetDirection, DeltaTime, LagSmoothingTime);
		AimDirection = AimDirection.GetSafeNormal(UE_SMALL_NUMBER, TargetDirection);

		// Only a safety net for spinning on the spot: the spring can trail well past Max Lag Angle, but not halfway round.
		LimitLag(AimDirection, AimDirectionRate, TargetDirection, FMath::Max(MaxLagAngle * 2.5f, 1.0f));
	}

	// The light shows the spring's lag up to most of Max Lag Angle, then eases into it. Easing only what's shown keeps
	// fast turns from stopping dead against a limit, and lets the light swing back as soon as the turn ends.
	const FVector LightDirection = EaseLag(AimDirection, TargetDirection, MaxLagAngle);

	// Roll follows the view, so a light function or IES pattern doesn't spin.
	SetWorldLocationAndRotation(LightLocation, FRotationMatrix::MakeFromXZ(LightDirection, ViewUp).ToQuat());
}

void UHadesFlashlightComponent::CreateBeam()
{
	AActor* Owner = GetOwner();
	if (BeamComponent || !BeamMaterial || !BeamProxyMesh || !Light || !Owner)
	{
		return;
	}

	BeamComponent = NewObject<UStaticMeshComponent>(Owner,
		MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("FlashlightBeam")), RF_Transient);
	BeamComponent->SetupAttachment(Light);
	BeamComponent->SetMobility(EComponentMobility::Movable);
	BeamComponent->SetStaticMesh(BeamProxyMesh);

	// It's only there to be drawn on.
	BeamComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamComponent->SetGenerateOverlapEvents(false);
	BeamComponent->SetCanEverAffectNavigation(false);
	BeamComponent->SetCastShadow(false);
	BeamComponent->bAffectDynamicIndirectLighting = false;
	BeamComponent->bAffectDistanceFieldLighting = false;
	BeamComponent->bVisibleInReflectionCaptures = false;
	BeamComponent->bVisibleInRayTracing = false;

	// The material reads the light's position from the mesh's (Object Position) and its direction from the mesh's
	// local Z (Object Orientation), so the box is centred on the light with Z along the beam.
	BeamComponent->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));

	BeamComponent->RegisterComponent();

	BeamMaterialInstance = BeamComponent->CreateDynamicMaterialInstance(0, BeamMaterial);

	BeamProxyRadius = -1.0f;
	BeamProxyConeAngle = -1.0f;
	bBeamParametersSet = false;
}

void UHadesFlashlightComponent::DestroyBeam()
{
	if (BeamComponent)
	{
		BeamComponent->DestroyComponent();
		BeamComponent = nullptr;
	}
	BeamMaterialInstance = nullptr;
}

void UHadesFlashlightComponent::UpdateBeam()
{
	if (!BeamComponent || !BeamMaterialInstance || !Light)
	{
		return;
	}

	// Follow the light if something else turns it off or on.
	const bool bLightVisible = Light->IsVisible();
	if (BeamComponent->IsVisible() != bLightVisible)
	{
		BeamComponent->SetVisibility(bLightVisible);
	}

	// The same cone limits Unreal applies to the light.
	const float Radius = FMath::Max(Light->AttenuationRadius, 1.0f);
	const float InnerConeRadians = FMath::DegreesToRadians(FMath::Clamp(Light->InnerConeAngle, 0.0f, 89.0f));
	const float OuterConeRadians = FMath::Clamp(FMath::DegreesToRadians(Light->OuterConeAngle),
		InnerConeRadians + 0.001f, FMath::DegreesToRadians(89.0f) + 0.001f);

	if (Radius != BeamProxyRadius || OuterConeRadians != BeamProxyConeAngle)
	{
		BeamProxyRadius = Radius;
		BeamProxyConeAngle = OuterConeRadians;

		// Big enough to hold the cone out to the attenuation radius, with a little margin.
		const double HalfWidth = Radius * FMath::Sin(OuterConeRadians) * 1.05 + 10.0;
		const double HalfLength = Radius * 1.05 + 10.0;
		const FVector MeshExtent = BeamProxyMesh->GetBoundingBox().GetExtent().ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
		BeamComponent->SetRelativeScale3D(FVector(HalfWidth / MeshExtent.X, HalfWidth / MeshExtent.Y, HalfLength / MeshExtent.Z));
	}

	const float CosOuter = FMath::Cos(OuterConeRadians);
	const float CosInner = FMath::Cos(InnerConeRadians);
	const FLinearColor Shape(Radius, CosOuter, 1.0f / (CosInner - CosOuter), FMath::Max(BeamFadeInDistance, 1.0f));

	// The light's own brightness and colour, so the beam dims and tints with it. Haze Density is per metre; the shader
	// works in centimetres. Kept just above zero so dust still shows with no haze.
	const float HazePerCm = FMath::Max(HazeDensity, 1e-6f) * 0.01f;
	FLinearColor Color = FLinearColor(Light->LightColor) * Light->ComputeLightBrightness();
	if (Light->bUseTemperature)
	{
		Color *= FLinearColor::MakeFromColorTemperature(Light->Temperature);
	}
	Color *= HazePerCm * FMath::Max(BeamIntensity, 0.0f);
	Color.A = 1.0f;

	// The shader lights dust with the same colour, so the haze is divided back out. Specks reflect like a mid-grey surface (0.5 / pi).
	const float DustBrightness = FMath::Max(DustAmount, 0.0f) * (0.5f / UE_PI) / HazePerCm;
	const float CosHotspot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HotspotAngle, 0.5f, 30.0f)));
	const FLinearColor Look(FMath::Clamp(HazeNoiseAmount, 0.0f, 1.0f), DustBrightness, CosHotspot, FMath::Clamp(SpillBrightness, 0.0f, 1.0f));

	if (!bBeamParametersSet || !Shape.Equals(LastBeamShape))
	{
		BeamMaterialInstance->SetVectorParameterValue(BeamShapeParameter, Shape);
		LastBeamShape = Shape;
	}
	if (!bBeamParametersSet || !Color.Equals(LastBeamColor))
	{
		BeamMaterialInstance->SetVectorParameterValue(BeamColorParameter, Color);
		LastBeamColor = Color;
	}
	if (!bBeamParametersSet || !Look.Equals(LastBeamLook))
	{
		BeamMaterialInstance->SetVectorParameterValue(BeamLookParameter, Look);
		LastBeamLook = Look;
	}

	// The shader moves the haze and dust from the wind and the game clock, so they stop when the game pauses.
	const UWorld* World = GetWorld();
	BeamMaterialInstance->SetVectorParameterValue(BeamNoiseParameter, FLinearColor(
		static_cast<float>(HazeWind.X), static_cast<float>(HazeWind.Y), static_cast<float>(HazeWind.Z),
		World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f));

	bBeamParametersSet = true;
}
