#pragma once

#include "CoreMinimal.h"

#include "UObject/Interface.h"

#include "HadesAbilitySystemInterface.generated.h"


UINTERFACE(MinimalAPI)
class UHadesAbilitySystemInterface : public UInterface
{
	GENERATED_BODY()
};


class HADES_API IHadesAbilitySystemInterface
{
	GENERATED_BODY()

public:

	virtual class UHadesAbilitySystemComponent* GetHadesAbilitySystemComponent() const = 0;
};