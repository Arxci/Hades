#pragma once

#include "CoreMinimal.h"

#include "GameFramework/SaveGame.h"

#include "GameplayTagContainer.h"

#include "HadesPlayerSettings.generated.h"

class ULocalPlayer;


/** A local player's preferences. They last the whole session; save them with Async Save Game To Slot For Local Player to keep them for the next one. */
UCLASS()
class HADES_API UHadesPlayerSettings : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:
	/** Null without a local player. */
	static UHadesPlayerSettings* Get(const ULocalPlayer* LocalPlayer);

	UFUNCTION(BlueprintPure, Category = "Settings|Input")
	bool IsInputToggleEnabled(UPARAM(meta = (Categories = "Input")) FGameplayTag InputTag) const { return ToggleInputTags.HasTagExact(InputTag); }

	UFUNCTION(BlueprintCallable, Category = "Settings|Input")
	void SetInputToggleEnabled(UPARAM(meta = (Categories = "Input")) FGameplayTag InputTag, bool bEnabled);

private:
	UPROPERTY()
	FGameplayTagContainer ToggleInputTags;
};
