#include "HadesPlayerSettingsSubsystem.h"

#include "Engine/LocalPlayer.h"

#include "Hades/Player/HadesPlayerSettings.h"

void UHadesPlayerSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Settings = Cast<UHadesPlayerSettings>(ULocalPlayerSaveGame::LoadOrCreateSaveGameForLocalPlayer(
		UHadesPlayerSettings::StaticClass(), GetLocalPlayer(), TEXT("PlayerSettings")));
}
