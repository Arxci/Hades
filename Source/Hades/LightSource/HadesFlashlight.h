#pragma once

#include "CoreMinimal.h"

#include "Hades/LightSource/HadesLightSource.h"

#include "HadesFlashlight.generated.h"

class UHadesFlashlightComponent;
class USpotLightComponent;


/**
 * A flashlight's light source: a spot light held as a hand holds a flashlight, trailing the view and aimed at what's under the crosshair,
 * with its beam drawn through the air. It starts with the spot light the hero used to carry. Make a Blueprint of it to set its beam material,
 * IES profile and battery, and use that as the Actor Class of an Actor piece on the flashlight item's cosmetics, attached to ViewRoot.
 */
UCLASS(Blueprintable)
class HADES_API AHadesFlashlight : public AHadesLightSource
{
	GENERATED_BODY()

public:
	AHadesFlashlight();

	UHadesFlashlightComponent* GetFlashlight() const { return Flashlight; }
	USpotLightComponent* GetSpotLight() const { return SpotLight; }

protected:
	/** Holds Spot Light the way a hand holds a flashlight, and draws its beam with a Beam Material. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight")
	TObjectPtr<UHadesFlashlightComponent> Flashlight;

	/** Brightness, cone, colour, IES profile and light function go here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight")
	TObjectPtr<USpotLightComponent> SpotLight;
};
