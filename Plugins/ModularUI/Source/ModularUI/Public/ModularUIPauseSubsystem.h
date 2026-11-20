#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"

#include "ModularUIPauseSubsystem.generated.h"

class UPostProcessComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularUIPauseSignature, bool, bPaused);


/**
 * Keeps the world paused while anything asks it to, such as an open menu, and starts it again once nothing does. It can also fade a post
 * process in while the world is paused, such as one that greys it out, and back out as it starts again. Modular Activatable Widgets with
 * Pause Game ask it for them. It pauses through the first local player, so it suits single player games.
 */
UCLASS()
class MODULARUI_API UModularUIPauseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UModularUIPauseSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem Interface
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/** The world stays paused until Requester stops asking, or is destroyed. Asking again changes nothing. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Pause")
	void AddPauseRequest(UObject* Requester);

	UFUNCTION(BlueprintCallable, Category = "Modular UI|Pause")
	void RemovePauseRequest(UObject* Requester);

	/** Whether anything is asking for the world to be paused. */
	UFUNCTION(BlueprintPure, Category = "Modular UI|Pause")
	bool IsPauseRequested() const { return bPauseRequested; }

	/**
	 * Fades Post Process in while the world is paused for a request, over Fade In Time, and back out over Fade Out Time as it starts again.
	 * The fade runs in real time, as the world is paused while it fades in. Its Blend Weight is how strong it gets, and it's off the rest of
	 * the time. None stops fading the one set before, and turns it off.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Pause")
	void SetPausedPostProcess(UPostProcessComponent* PostProcess, float FadeInTime = 0.15f, float FadeOutTime = 0.2f);

	/** The world was paused for requests, or started again once they were gone. */
	UPROPERTY(BlueprintAssignable, Category = "Modular UI|Pause")
	FModularUIPauseSignature OnPauseChanged;

protected:
	//~ Begin UWorldSubsystem Interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

private:
	void UpdatePause();

	/** Fades the paused post process from the core ticker, as timers, timelines and Delay all wait for the world while it's paused. */
	void StartFade();
	bool TickFade(float DeltaTime);

	TArray<TWeakObjectPtr<const UObject>> PauseRequests;
	bool bPauseRequested = false;

	TWeakObjectPtr<UPostProcessComponent> PausedPostProcess;

	/** The Blend Weight the paused post process was given, and how far it has faded in, from 0 to 1. */
	float PausedBlendWeight = 1.0f;
	float FadeAlpha = 0.0f;
	float FadeInSeconds = 0.15f;
	float FadeOutSeconds = 0.2f;
	FTSTicker::FDelegateHandle FadeHandle;
};
