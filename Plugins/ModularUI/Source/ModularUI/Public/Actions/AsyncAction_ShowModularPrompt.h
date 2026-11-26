#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "Widgets/ModularPromptTypes.h"

#include "AsyncAction_ShowModularPrompt.generated.h"

class APlayerController;
class UModularPromptWidget;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularPromptResultSignature, int32, Count);


/** Asks the player something in a prompt dialog and waits for the answer, such as "Quit to the main menu?" before quitting. */
UCLASS()
class MODULARUI_API UAsyncAction_ShowModularPrompt : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Pushes a Prompt Class dialog onto the player's layer, such as UI.Layer.Modal, showing Request, then fires Confirmed with the count
	 * chosen, 0 for a confirmation, or Cancelled. Fires Cancelled at once when there's no such layer.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Modular UI|Prompt", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Show Modular Prompt"))
	static UAsyncAction_ShowModularPrompt* ShowModularPrompt(APlayerController* PlayerController, UPARAM(meta = (AllowAbstract = false)) TSubclassOf<UModularPromptWidget> PromptClass,
		UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, const FModularPromptRequest& Request);

	//~ Begin UBlueprintAsyncActionBase Interface
	virtual void Activate() override;
	//~ End UBlueprintAsyncActionBase Interface

	UPROPERTY(BlueprintAssignable)
	FModularPromptResultSignature Confirmed;

	UPROPERTY(BlueprintAssignable)
	FModularPromptResultSignature Cancelled;

private:
	void HandleAnswered(bool bConfirmed, int32 Count);

	TWeakObjectPtr<APlayerController> PlayerController;
	TSubclassOf<UModularPromptWidget> PromptClass;
	FGameplayTag LayerTag;
	FModularPromptRequest Request;
};
