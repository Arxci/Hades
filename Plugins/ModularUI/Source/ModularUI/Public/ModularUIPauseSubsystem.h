#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"

#include "ModularUIPauseSubsystem.generated.h"

class APostProcessVolume;
class UPostProcessComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularUIPauseSignature, bool, bPaused);


/**
 * Keeps the world paused while anything asks it to, such as an open menu, and starts it again once nothing does. It also fades a look in
 * over the world while it's paused, such as one that greys it out, and back out as it starts again: Project Settings > Plugins > Modular UI >
 * Paused Look, shown with a post process volume of its own, so nothing needs adding to an actor. Modular Activatable Widgets with Pause Game
 * ask it for the pause. It pauses through the first local player, so it suits single player games.
 *
 * To check the paused look, type ModularUI.Pause.ForceLook in the console while playing: it shows the look without pausing. If nothing
 * changes then, the look itself isn't reaching the screen, such as when a camera's own post process settings override the same settings.
 * ModularUI.Pause.Status logs who asks for the pause and the look's state.
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
	 * Fades Post Process in while the world is paused, instead of Project Settings' Paused Look, such as one on the player's camera actor.
	 * It fades in over Fade In Time, in real time, and back out over Fade Out Time. Its Blend Weight is how strong it gets, and it's off the
	 * rest of the time. None goes back to Paused Look.
	 * Unreal doesn't apply post process components on actors hidden in game, as every controller is, so don't use one on a player
	 * controller. A camera's own post process settings are applied after every post process component and volume, so a camera that ticks
	 * a setting the look uses, such as Saturation, hides that part of the look.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Pause")
	void SetPausedPostProcess(UPostProcessComponent* PostProcess, float FadeInTime = 0.15f, float FadeOutTime = 0.2f);

	/** The post process given to Set Paused Post Process, or None while Project Settings' Paused Look is used. */
	UFUNCTION(BlueprintPure, Category = "Modular UI|Pause")
	UPostProcessComponent* GetPausedPostProcess() const;

	/**
	 * Shows the paused post process as if the world were paused, without pausing it, such as to tune the look or to check that it reaches
	 * the screen at all. Off, it follows the pause requests again. The console command ModularUI.Pause.ForceLook does the same.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Pause")
	void SetPausedLookForced(bool bForced);

	UFUNCTION(BlueprintPure, Category = "Modular UI|Pause")
	bool IsPausedLookForced() const { return bPausedLookForced; }

	/** Logs who is asking for the pause and the paused post process's state, such as its Blend Weight and how far it has faded in. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Pause")
	void LogStatus();

	/** The world was paused for requests, or started again once they were gone. */
	UPROPERTY(BlueprintAssignable, Category = "Modular UI|Pause")
	FModularUIPauseSignature OnPauseChanged;

protected:
	//~ Begin UWorldSubsystem Interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

private:
	void UpdatePause();

	/** Fades the paused look from the core ticker, as timers, timelines and Delay all wait for the world while it's paused. */
	void StartFade();
	bool TickFade(float DeltaTime);

	/** Shows the fade on the post process given to Set Paused Post Process, or else on the volume for Paused Look. False when there's neither. */
	bool ApplyFade();
	float GetFadeInTime() const;
	float GetFadeOutTime() const;

	/** The volume showing Project Settings' Paused Look, spawned the first time it's needed. Null without a Paused Look. */
	APostProcessVolume* FindOrSpawnPausedLookVolume();

	/** Warns about a post process that can't show anything as it is, such as one on a hidden actor, or with a Blend Weight of 0. */
	void WarnIfNothingShows(const UPostProcessComponent& PostProcess) const;

	TArray<TWeakObjectPtr<const UObject>> PauseRequests;
	bool bPauseRequested = false;
	bool bPausedLookForced = false;

	TWeakObjectPtr<UPostProcessComponent> PausedPostProcess;
	TWeakObjectPtr<APostProcessVolume> PausedLookVolume;

	/** The Blend Weight the paused post process was given, and how far the look has faded in, from 0 to 1. */
	float PausedBlendWeight = 1.0f;
	float FadeAlpha = 0.0f;
	float FadeInSeconds = 0.15f;
	float FadeOutSeconds = 0.2f;
	FTSTicker::FDelegateHandle FadeHandle;
};
