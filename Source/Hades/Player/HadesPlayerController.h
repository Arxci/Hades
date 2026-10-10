#pragma once

#include "CoreMinimal.h"

#include "GameFramework/PlayerController.h"

#include "HadesPlayerController.generated.h"

class UInputMappingContext;


UCLASS()
class HADES_API AHadesPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AHadesPlayerController();

protected:
	virtual void SetupInputComponent() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
};
