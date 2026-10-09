#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Character.h"

#include "HadesCharacterBase.generated.h"

class UHadesCharacterMovementComponent;

UCLASS()
class HADES_API AHadesCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AHadesCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UHadesCharacterMovementComponent> HadesCharacterMovement;
};
