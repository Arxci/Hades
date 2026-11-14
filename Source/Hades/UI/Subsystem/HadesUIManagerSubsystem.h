#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "GameUIManagerSubsystem.h"

#include "HadesUIManagerSubsystem.generated.h"

class FNavigationConfig;

/**
 * CommonGame needs a concrete UI manager. Its DefaultUIPolicyClass is set in DefaultGame.ini.
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

	/** Put back when the game ends, as the editor shares it. */
	TSharedPtr<FNavigationConfig> PreviousNavigationConfig;
};
