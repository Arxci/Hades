#pragma once

#include "CoreMinimal.h"

#include "CommonGameInstance.h"

#include "HadesGameInstance.generated.h"

/** CommonGame needs a concrete game instance. It hands each local player to the UI manager, which gives them a root layout. */
UCLASS()
class HADES_API UHadesGameInstance : public UCommonGameInstance
{
	GENERATED_BODY()
};
