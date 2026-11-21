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
	UpdateBeam(DeltaTime);
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
		UpdateBeam(0.0f);
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
			UpdateBeam(0.0f);
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

	// Where the hand holds it, pulled back toward the eye if that would put it in a wall.
	FVector LightLocation = ViewLocation + ViewRotation.RotateVector(HandOffset);
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

	// Trail behind the view, but never by more than the max angle.
	if (bSnap || LagSmoothingTime <= 0.0f)
	{
		AimDirection = TargetDirection;
		AimDirectionRate = FVector::ZeroVector;
	}
	else
	{
		CriticallyDampedStep(AimDirection, AimDirectionRate, TargetDirection, DeltaTime, LagSmoothingTime);
		AimDirection = AimDirection.GetSafeNormal(UE_SMALL_NUMBER, TargetDirection);

		if (FVector::DotProduct(AimDirection, TargetDirection) < FMath::Cos(FMath::DegreesToRadians(MaxLagAngle)))
		{
			const FVector Axis = FVector::CrossProduct(TargetDirection, AimDirection).GetSafeNormal();
			if (Axis.IsZero())
			{
				AimDirection = TargetDirection;
				AimDirectionRate = FVector::ZeroVector;
			}
			else
			{
				// Drag it along at the limit, and drop the part of its motion that would carry it further behind.
				AimDirection = TargetDirection.RotateAngleAxis(MaxLagAngle, Axis);
				const FVector Behind = (AimDirection - TargetDirection).GetSafeNormal();
				const double SpeedBehind = FVector::DotProduct(AimDirectionRate, Behind);
				if (SpeedBehind > 0.0)
				{
					AimDirectionRate -= Behind * SpeedBehind;
				}
			}
		}
	}

	// Roll follows the view, so a light function or IES pattern doesn't spin.
	SetWorldLocationAndRotation(LightLocation, FRotationMatrix::MakeFromXZ(AimDirection, ViewUp).ToQuat());
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

void UHadesFlashlightComponent::UpdateBeam(float DeltaTime)
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

	// The light's own brightness and colour, so the beam dims and tints with it. Haze Density is per metre; the shader works in centimetres.
	FLinearColor Color = FLinearColor(Light->LightColor) * Light->ComputeLightBrightness();
	if (Light->bUseTemperature)
	{
		Color *= FLinearColor::MakeFromColorTemperature(Light->Temperature);
	}
	Color *= FMath::Max(HazeDensity, 0.0f) * 0.01f * FMath::Max(BeamIntensity, 0.0f);
	Color.A = 1.0f;

	const FLinearColor Look(FMath::Clamp(HazeAnisotropy, -0.9f, 0.9f), FMath::Clamp(HazeNoiseAmount, 0.0f, 1.0f), 0.0f, 0.0f);

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

	// Drifts with the game clock, so it stops when the game pauses.
	HazeOffset += HazeWind * DeltaTime;
	BeamMaterialInstance->SetVectorParameterValue(BeamNoiseParameter, FLinearColor(
		static_cast<float>(HazeOffset.X), static_cast<float>(HazeOffset.Y), static_cast<float>(HazeOffset.Z),
		1.0f / FMath::Max(HazeNoiseSize, 1.0f)));

	bBeamParametersSet = true;
}
