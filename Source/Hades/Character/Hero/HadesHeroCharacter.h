#pragma once

#include "CoreMinimal.h"

#include "Hades/Character//HadesCharacterBase.h"

#include "HadesHeroCharacter.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;


UCLASS()
class HADES_API AHadesHeroCharacter : public AHadesCharacterBase
{
	GENERATED_BODY()

public:
	AHadesHeroCharacter(const FObjectInitializer& ObjectInitializer  = FObjectInitializer::Get());
	
protected:
	//~~Start Components
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USceneComponent> ViewRoot;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<USceneComponent> ViewSource;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	//~~Stop Components
};
