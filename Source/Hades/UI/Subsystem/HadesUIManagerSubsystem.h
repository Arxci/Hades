#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "GameUIManagerSubsystem.h"

#include "HadesUIManagerSubsystem.generated.h"


/**
 * CommonGame needs a concrete UI manager. Its DefaultUIPolicyClass is set in DefaultGame.ini.
 * It hands CommonGame's root layout to Modular UI, so widgets that open on Modular UI layers, such as item menus and prompt dialogs, open on
 * the root layout's layers of the same tags. The root layout hides while the HUD is hidden, such as with the ShowHUD console command.
 * Keyboard navigation, such as Tab and the arrow keys moving focus, is set in Project Settings > Modular UI.
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
