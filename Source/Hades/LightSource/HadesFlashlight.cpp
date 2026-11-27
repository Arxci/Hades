#include "HadesFlashlight.h"

#include "Components/SpotLightComponent.h"

#include "Hades/Flashlight/HadesFlashlightComponent.h"

AHadesFlashlight::AHadesFlashlight()
{
	Flashlight = CreateDefaultSubobject<UHadesFlashlightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(GetRootComponent());

	// A small LED flashlight, as the hero carried before lights were items.
	SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
	SpotLight->SetupAttachment(Flashlight);
	SpotLight->SetMobility(EComponentMobility::Movable);
	SpotLight->IntensityUnits = ELightUnits::Candelas;
	SpotLight->Intensity = 2000.0f;
	SpotLight->AttenuationRadius = 1500.0f;
	SpotLight->InnerConeAngle = 21.0f;
	SpotLight->OuterConeAngle = 24.0f;
	SpotLight->SourceRadius = 2.0f;
	SpotLight->bUseTemperature = true;
	SpotLight->Temperature = 6800.0f;
	SpotLight->ContactShadowLength = 0.02f;

	// The flashlight component draws the beam; volumetric fog's version smears as the view turns.
	SpotLight->VolumetricScatteringIntensity = 0.0f;
}
