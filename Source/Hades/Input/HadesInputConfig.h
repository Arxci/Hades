#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"

#include "GameplayTagContainer.h"

#include "HadesInputConfig.generated.h"

class UInputAction;


USTRUCT(BlueprintType)
struct FHadesInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;
};


UCLASS(BlueprintType, Const)
class HADES_API UHadesInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const UInputAction* FindInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FHadesInputAction> InputActions;
};
