#pragma once

#include "CoreMinimal.h"

#include "Subsystems/LocalPlayerSubsystem.h"

#include "HadesPlayerSettingsSubsystem.generated.h"

class UHadesPlayerSettings;


/** Loads a local player's settings when the player is added and keeps them across level loads. */
UCLASS()
class HADES_API UHadesPlayerSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem Interface

	UFUNCTION(BlueprintPure, Category = "Settings")
	UHadesPlayerSettings* GetSettings() const { return Settings; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UHadesPlayerSettings> Settings;
};
