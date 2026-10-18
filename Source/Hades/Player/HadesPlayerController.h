#pragma once

#include "CoreMinimal.h"

#include "GameFramework/PlayerController.h"

#include "GameplayTagContainer.h"

#include "HadesPlayerController.generated.h"

class UInputMappingContext;


UCLASS()
class HADES_API AHadesPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AHadesPlayerController();
	
	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsInputToggleEnabled(FGameplayTag InputTag) const { return ToggleInputTags.HasTagExact(InputTag); }
	
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetInputToggleEnabled(FGameplayTag InputTag, bool bEnabled);

protected:
	virtual void SetupInputComponent() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTagContainer ToggleInputTags;
};