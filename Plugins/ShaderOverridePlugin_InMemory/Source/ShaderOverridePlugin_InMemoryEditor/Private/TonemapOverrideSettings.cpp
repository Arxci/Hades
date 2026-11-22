#include "TonemapOverrideSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogTonemapOverrideSettings, Log, All);

#define LOCTEXT_NAMESPACE "TonemapOverrideSettings"

UTonemapOverrideSettings::UTonemapOverrideSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("TonemapOverride");
}

FString UTonemapOverrideSettings::GetConfigHeaderPath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ShaderOverridePlugin_InMemory"));
	const FString BaseDir = Plugin.IsValid()
		? Plugin->GetBaseDir()
		: FPaths::ProjectPluginsDir() / TEXT("ShaderOverridePlugin_InMemory");
	return BaseDir / TEXT("Shaders/TonemapOverrideConfig.ush");
}

FString UTonemapOverrideSettings::BuildConfigHeader() const
{
	// Must stay byte-identical to the shipped Shaders/TonemapOverrideConfig.ush for default settings
	FString Text;
	Text += TEXT("// GENERATED FILE - edit via Project Settings > Plugins > Tonemap Override.\n");
	Text += TEXT("// Manual edits are overwritten when those settings change or the editor starts.\n");
	Text += TEXT("\n");
	Text += FString::Printf(TEXT("#define CUSTOM_TONEMAP_MODE %d\n"), static_cast<int32>(Tonemapper));
	Text += FString::Printf(TEXT("#define AGX_LOOK %d\n"), static_cast<int32>(AgxLook));
	Text += FString::Printf(TEXT("#define SPLIT_TONE_PRESET %d\n"), static_cast<int32>(SplitTone));
	Text += FString::Printf(TEXT("#define HUE_PRESERVE_PERCENT %d\n"), FMath::Clamp(HuePreservationPercent, 0, 100));
	return Text;
}

bool UTonemapOverrideSettings::ApplyToShaders(bool bRecompile) const
{
	const FString Path = GetConfigHeaderPath();
	const FString NewText = BuildConfigHeader();

	// Ignore line-ending differences (e.g. git autocrlf on Windows) so we don't recompile for nothing
	FString OldText;
	FFileHelper::LoadFileToString(OldText, *Path);
	if (OldText.Replace(TEXT("\r\n"), TEXT("\n")) == NewText)
	{
		return false;
	}

	if (!FFileHelper::SaveStringToFile(NewText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTonemapOverrideSettings, Warning, TEXT("Could not write %s (is it read-only?)"), *Path);
		return false;
	}

	UE_LOG(LogTonemapOverrideSettings, Log, TEXT("Updated %s"), *Path);

	if (bRecompile && GEngine)
	{
		// Implemented by the runtime module: reloads overrides and recompiles changed shaders
		GEngine->Exec(nullptr, TEXT("ShaderOverride.Reload"));
	}
	return true;
}

#if WITH_EDITOR
void UTonemapOverrideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Slider drags fire many interactive events; only recompile for the final value
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		ApplyToShaders(true);
	}
}

FText UTonemapOverrideSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "Tonemap Override");
}

FText UTonemapOverrideSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription",
		"Choose the tonemapper and looks baked into the color grading LUT. Changes recompile the LUT shader. "
		"Per-volume controls live in Post Process Volumes (Film > Slope/Toe/Shoulder/Black Clip/White Clip, "
		"Color Grading > Misc > Blue Correction/Expand Gamut). See the plugin's README.md.");
}
#endif

#undef LOCTEXT_NAMESPACE
