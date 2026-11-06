#include "HadesPlayerSettings.h"

#include "Engine/LocalPlayer.h"

#include "Hades/Player/HadesPlayerSettingsSubsystem.h"

UHadesPlayerSettings* UHadesPlayerSettings::Get(const ULocalPlayer* LocalPlayer)
{
	const UHadesPlayerSettingsSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UHadesPlayerSettingsSubsystem>(LocalPlayer);
	return Subsystem ? Subsystem->GetSettings() : nullptr;
}

void UHadesPlayerSettings::SetInputToggleEnabled(FGameplayTag InputTag, bool bEnabled)
{
	if (bEnabled)
	{
		ToggleInputTags.AddTag(InputTag);
	}
	else
	{
		ToggleInputTags.RemoveTag(InputTag);
	}
}
