#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "GameUIManagerSubsystem.h"

#include "HadesUIManagerSubsystem.generated.h"

/**
 * CommonGame needs a concrete UI manager. Its DefaultUIPolicyClass is set in DefaultGame.ini.
 * Hides the root layout while the player's HUD is hidden.
 */
UCLASS()
class HADES_API UHadesUIManagerSubsystem : public UGameUIManagerSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

private:
	bool Tick(float DeltaTime);
	void SyncRootLayoutVisibilityToShowHUD();

	FTSTicker::FDelegateHandle TickHandle;
};
