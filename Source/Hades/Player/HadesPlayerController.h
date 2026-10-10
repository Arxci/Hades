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

	/** Whether the player prefers toggle over hold for this Hold-mode ability input. */
	UFUNCTION(BlueprintPure, Category = "Input")
	bool IsInputToggleEnabled(FGameplayTag InputTag) const { return ToggleInputTags.HasTagExact(InputTag); }

	/** Switch a Hold-mode ability input (Input.Run, Input.Crouch) between hold and toggle. A player setting. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetInputToggleEnabled(FGameplayTag InputTag, bool bEnabled);

protected:
	virtual void SetupInputComponent() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	/** Hold-mode ability inputs the player has switched to toggle. Lives here, not on the pawn, so it survives respawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTagContainer ToggleInputTags;
};