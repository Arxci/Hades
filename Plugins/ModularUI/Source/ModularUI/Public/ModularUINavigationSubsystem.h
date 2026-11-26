#pragma once

#include "CoreMinimal.h"

#include "Subsystems/GameInstanceSubsystem.h"

#include "ModularUINavigationSubsystem.generated.h"

class FNavigationConfig;


/**
 * Sets up how keys move focus around the UI while the game runs, as Project Settings > Plugins > Modular UI > Navigation says, such as
 * no Tab navigation so Tab is free for the game. Puts Unreal's own setup back as the game ends, as the editor shares it. Does nothing while
 * every Navigation setting is left on.
 */
UCLASS()
class MODULARUI_API UModularUINavigationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

private:
	/** Put back when the game ends. */
	TSharedPtr<FNavigationConfig> PreviousNavigationConfig;
};
