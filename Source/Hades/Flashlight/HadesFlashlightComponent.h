#pragma once

#include "CoreMinimal.h"

#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"

#include "HadesFlashlightComponent.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Holds the spot light attached to it the way a hand holds a flashlight: off to the side of the eye so shadows show,
 * trailing the view a little, aimed at whatever is under the crosshair, and kept out of walls.
 * It places itself every frame, so attach it to something that follows the pawn but doesn't pitch with the view, like the hero's ViewRoot.
 *
 * With a Beam Material it also draws the beam through the air, without the lag volumetric fog has (see Shaders/FlashlightBeam.ush).
 * Set the light's Volumetric Scattering Intensity to 0 so fog doesn't draw a second, smeared beam.
 */
UCLASS(ClassGroup = (Lights), meta = (BlueprintSpawnableComponent))
class HADES_API UHadesFlashlightComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHadesFlashlightComponent();

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void SetFlashlightOn(bool bOn);

	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void ToggleFlashlight() { SetFlashlightOn(!bFlashlightOn); }

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	/** The spot light this aims: the first one attached to it. */
	UFUNCTION(BlueprintPure, Category = "Flashlight")
	USpotLightComponent* GetLight() const { return Light; }

	/** Swaps the material that draws the beam. Null removes the beam. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight|Beam")
	void SetBeamMaterial(UMaterialInterface* NewBeamMaterial);

	/** Jumps straight to where the light should point, without trailing. For teleports and camera cuts. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void SnapToView();

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flashlight")
	bool bStartOn = true;

	/** Where the hand holds the light, from the eye: X forward, Y right, Z up. Off to the side so shadows fall where you can see them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (Units = "cm"))
	FVector HandOffset = FVector(10.0, 18.0, -18.0);

	/** How long the light takes to catch up when you turn. 0 locks it to the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (ClampMin = "0", Units = "s"))
	float LagSmoothingTime = 0.06f;

	/** The furthest the light can fall behind the view during a fast turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (ClampMin = "0", ClampMax = "45", Units = "deg"))
	float MaxLagAngle = 12.0f;

	/** Aims the light at what the crosshair is on, so the hotspot lands there despite Hand Offset. Otherwise it points the way you look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement")
	bool bAimAtCrosshair = true;

	/** Anything closer is aimed at as if it were this far away, so the light doesn't swing hard across the view up close. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (EditCondition = "bAimAtCrosshair", ClampMin = "1", Units = "cm"))
	float MinAimDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (EditCondition = "bAimAtCrosshair", ClampMin = "1", Units = "cm"))
	float MaxAimDistance = 3000.0f;

	/** Smooths the aim when what's under the crosshair jumps nearer or further, like when looking past a door frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (EditCondition = "bAimAtCrosshair", ClampMin = "0", Units = "s"))
	float AimSmoothingTime = 0.15f;

	/** Pulls the light toward the eye, this far clear of anything between them, so it can't end up behind a wall. 0 turns the check off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (ClampMin = "0", Units = "cm"))
	float WallClearance = 5.0f;

	/** For the crosshair trace and the wall check. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Draws the beam through the air. Empty means no beam. Shaders/FlashlightBeam.ush describes how to build it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flashlight|Beam")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	/** How much of the light the air scatters, per metre. Around 0.003 is faintly dusty, 0.01 a dusty room, 0.05 and up is fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0", UIMax = "0.1"))
	float HazeDensity = 0.01f;

	/** Scales the beam's brightness on top of Haze Density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0"))
	float BeamIntensity = 1.0f;

	/** Above 0, light keeps going the way it was headed, so a beam looks brighter pointed at you than away from you. 0 scatters it evenly. Dust and haze are around 0.2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "-0.9", ClampMax = "0.9"))
	float HazeAnisotropy = 0.2f;

	/** The beam fades in over this distance from the lens, so it doesn't wash over the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "1", Units = "cm"))
	float BeamFadeInDistance = 60.0f;

	/** How patchy the haze is. 0 is even. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0", ClampMax = "1"))
	float HazeNoiseAmount = 0.8f;

	/** Size of the patches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "1", Units = "cm"))
	float HazeNoiseSize = 100.0f;

	/** How fast the patches drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (Units = "cm/s"))
	FVector HazeWind = FVector(5.0, 2.0, 1.0);

	/** Mesh the beam is drawn on: a box centred on its pivot, scaled to hold the light's range. The shader works out the cone itself. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Flashlight|Beam")
	TObjectPtr<UStaticMesh> BeamProxyMesh;

private:
	bool GetViewPoint(FVector& OutLocation, FQuat& OutRotation) const;
	void UpdatePlacement(float DeltaTime, bool bSnap);
	void ApplyOnState();
	void CreateBeam();
	void DestroyBeam();
	void UpdateBeam(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> BeamComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamMaterialInstance;

	TWeakObjectPtr<UCameraComponent> ViewCamera;

	bool bFlashlightOn = false;

	FVector AimDirection = FVector::ForwardVector;
	FVector AimDirectionRate = FVector::ZeroVector;
	float AimDistance = 0.0f;
	float AimDistanceRate = 0.0f;

	FVector HazeOffset = FVector::ZeroVector;

	// What the beam was last set up with, so unchanged values aren't sent again.
	float BeamProxyRadius = -1.0f;
	float BeamProxyConeAngle = -1.0f;
	bool bBeamParametersSet = false;
	FLinearColor LastBeamShape;
	FLinearColor LastBeamColor;
	FLinearColor LastBeamLook;
};
