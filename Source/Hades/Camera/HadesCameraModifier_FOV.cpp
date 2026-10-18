#include "HadesCameraModifier_FOV.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

bool UHadesCameraModifier_FOV::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	InOutPOV.FOV += FOVOffset * static_cast<float>(UKismetMathLibrary::Ease(0.0f, 1.0f, Alpha, Easing));
	return false;
}

float UHadesCameraModifier_FOV::GetTargetAlpha()
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetViewTarget());
	return ASC && ASC->HasMatchingGameplayTag(ActiveTag) ? Super::GetTargetAlpha() : 0.0f;
}
