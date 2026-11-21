#pragma once

#include "CoreMinimal.h"

#include "Camera/CameraModifier.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

#include "HadesCameraModifier_AutoFocus.generated.h"

/**
 * Pulls depth of field focus onto whatever the player is looking at.
 */
UCLASS(Blueprintable)
class HADES_API UHadesCameraModifier_AutoFocus : public UCameraModifier
{
	GENERATED_BODY()

public:
	UHadesCameraModifier_AutoFocus(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UCameraModifier Interface
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	//~ End UCameraModifier Interface

	/** The distance the camera is focused at right now. */
	UFUNCTION(BlueprintPure, Category = "Auto Focus", meta = (ReturnDisplayName = "Distance (cm)"))
	float GetFocusDistance() const { return 1.0f / CurrentDiopters; }

protected:
	//~ Begin UCameraModifier Interface
	virtual float GetTargetAlpha() override;
	//~ End UCameraModifier Interface

	/** How far ahead to look for something to focus on. When nothing is hit, focus sits at Max Focus Distance. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (Units = "cm", ClampMin = "100"))
	float TraceDistance = 3000.0f;

	/** Radius of the focus probe. A few cm stops focus snapping through gaps, railings and fences. 0 uses a line. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (Units = "cm", ClampMin = "0"))
	float TraceRadius = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Closest the lens will focus, so hugging a wall doesn't pull focus to a few cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (Units = "cm", ClampMin = "10"))
	float MinFocusDistance = 40.0f;

	/** Focus distance when the probe hits nothing, such as down a long corridor or at the sky. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (Units = "cm", ClampMin = "100"))
	float MaxFocusDistance = 3000.0f;

	/** How quickly focus follows what you look at. 4 to 8 feels like an eye; lower is a slow, cinematic rack. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (ClampMin = "0.1"))
	float FocusSpeed = 6.0f;

	/** Also drive the aperture. Turn off if your camera or a volume sets Aperture (F-stop) itself. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus")
	bool bOverrideAperture = true;

	/**
	 * Aperture when fully blended in. Lower means more blur. At 2.8 the room behind anything within about a metre
	 * goes soft, while focus on things a few metres away keeps the whole view nearly sharp.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (EditCondition = "bOverrideAperture", ClampMin = "1.0", ClampMax = "32.0"))
	float Aperture = 2.8f;

	/** While the view target has any of these tags, the blur fades out (for example while aiming). */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus", meta = (Categories = "Character.State"))
	FGameplayTagContainer BlockedTags;

	/** Leave focus alone while the game is paused, so the paused look's own depth of field shows. */
	UPROPERTY(EditDefaultsOnly, Category = "Auto Focus")
	bool bSkipWhilePaused = true;

private:
	bool IsViewingThroughPlayerPawn() const;
	float TraceFocusDistance(const FMinimalViewInfo& POV) const;

	float CurrentDiopters = 1.0f / 3000.0f;
	bool bHasFocus = false;
};
