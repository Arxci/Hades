#include "ModularUISettings.h"

#include "UObject/PropertyPortFlags.h"

UModularUISettings::UModularUISettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("ModularUI");
}

bool UModularUISettings::HasPausedLook() const
{
	// Settings left as they are by default change nothing, whatever their weight.
	static const FPostProcessSettings DefaultSettings;
	return !FPostProcessSettings::StaticStruct()->CompareScriptStruct(&PausedLook, &DefaultSettings, PPF_None);
}
