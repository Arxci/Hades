#pragma once

#include "CoreMinimal.h"

#include "Camera/CameraModifier.h"
#include "GameplayTagContainer.h"
#include "Kismet/KismetMathLibrary.h"

#include "HadesCameraModifier_FOV.generated.h"


UCLASS(Abstract)
class HADES_API UHadesCameraModifier_FOV : public UCameraModifier
{
	GENERATED_BODY()

public:
	//~ Begin UCameraModifier Interface
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	//~ End UCameraModifier Interface

protected:
	//~ Begin UCameraModifier Interface
	virtual float GetTargetAlpha() override;
	//~ End UCameraModifier Interface

	/** Blends in while the view target has this tag, over Alpha In Time and Alpha Out Time. */
	UPROPERTY(EditDefaultsOnly, Category = "FOV", meta = (Categories = "Character.State"))
	FGameplayTag ActiveTag;

	UPROPERTY(EditDefaultsOnly, Category = "FOV", meta = (Units = "deg"))
	float FOVOffset = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "FOV")
	TEnumAsByte<EEasingFunc::Type> Easing = EEasingFunc::EaseInOut;
};
