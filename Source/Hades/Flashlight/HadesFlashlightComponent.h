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

	/**
	 * Where the hand holds the light, from the eye: X forward, Y right, Z up. The default is the left hand at chest height,
	 * so the beam rises into view from the bottom corner and shadows fall where you can see them. Use a positive Y for the right hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (Units = "cm"))
	FVector HandOffset = FVector(20.0, -22.0, -40.0);

	/**
	 * Keeps the light at least this far in front of the eye, measured level with the floor. Looking down, Hand Offset turns
	 * with the view and would otherwise swing back into the body, which still casts shadows while hidden. 0 turns this off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (ClampMin = "0", Units = "cm"))
	float BodyClearance = 25.0f;

	/** How long the light takes to catch up when you turn. 0 locks it to the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (ClampMin = "0", Units = "s"))
	float LagSmoothingTime = 0.1f;

	/**
	 * Roughly the furthest the light falls behind on very fast turns. It trails freely up to 70% of this, then eases toward it
	 * rather than stopping dead, so quick looks stay smooth.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Placement", meta = (ClampMin = "0", ClampMax = "60", Units = "deg"))
	float MaxLagAngle = 35.0f;

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

	/** How much of the light the air scatters, per metre. Around 0.01 is faint, 0.03 a dusty room, 0.05 and up is smoke or fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0", UIMax = "0.1"))
	float HazeDensity = 0.03f;

	/** Scales the beam's brightness, haze and dust together. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0"))
	float BeamIntensity = 1.0f;

	/**
	 * Half-angle of the bright core in the middle of the beam, which fades into the spill by 2.5 times this angle.
	 * The light's Outer Cone Angle is the beam's outer edge. FlashlightLED.ies gives the light the same core; change both together.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0.5", ClampMax = "30", Units = "deg"))
	float HotspotAngle = 6.0f;

	/** How bright the rest of the beam is next to its core. 1 is even all the way out to the edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0", ClampMax = "1"))
	float SpillBrightness = 0.3f;

	/** How brightly the dust specks within a few metres catch the light. 0 turns them off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0", UIMax = "4"))
	float DustAmount = 1.0f;

	/** The beam fades in over this distance from the lens. Short, since the lens is below the bottom of the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "1", Units = "cm"))
	float BeamFadeInDistance = 20.0f;

	/** How patchy the haze is. 0 is even. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Beam", meta = (ClampMin = "0", ClampMax = "1"))
	float HazeNoiseAmount = 0.5f;

	/** The air's average drift, which carries the haze and dust. Its direction wanders over time. 0 is still air, where specks just float in place. */
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
	void UpdateBeam();

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

	// What the beam was last set up with, so unchanged values aren't sent again.
	float BeamProxyRadius = -1.0f;
	float BeamProxyConeAngle = -1.0f;
	bool bBeamParametersSet = false;
	FLinearColor LastBeamShape;
	FLinearColor LastBeamColor;
	FLinearColor LastBeamLook;
};
