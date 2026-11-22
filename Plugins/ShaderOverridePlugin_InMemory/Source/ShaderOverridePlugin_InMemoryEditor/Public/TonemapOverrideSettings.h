#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TonemapOverrideSettings.generated.h"

UENUM()
enum class ETonemapOverrideMode : uint8
{
	EngineDefault = 0 UMETA(DisplayName = "Engine Default (ACES)"),
	AgX           = 1 UMETA(DisplayName = "AgX"),
	Reinhard      = 2 UMETA(DisplayName = "Reinhard"),
	Linear        = 3 UMETA(DisplayName = "Linear (no tone curve)"),
	PBRNeutral    = 4 UMETA(DisplayName = "Khronos PBR Neutral"),
	Hable         = 5 UMETA(DisplayName = "Hable (Uncharted 2)"),
	Uchimura      = 6 UMETA(DisplayName = "Uchimura (Gran Turismo)"),
	Lottes        = 7 UMETA(DisplayName = "Lottes (AMD)"),
	ACESFitted    = 8 UMETA(DisplayName = "ACES Fitted (Narkowicz)")
};

UENUM()
enum class ETonemapOverrideAgxLook : uint8
{
	None   = 0 UMETA(DisplayName = "None"),
	Golden = 1 UMETA(DisplayName = "Golden"),
	Punchy = 2 UMETA(DisplayName = "Punchy")
};

UENUM()
enum class ETonemapOverrideSplitTone : uint8
{
	None          = 0 UMETA(DisplayName = "None"),
	TealOrange    = 1 UMETA(DisplayName = "Teal & Orange"),
	SicklyGreen   = 2 UMETA(DisplayName = "Sickly Green"),
	ColdMoonlight = 3 UMETA(DisplayName = "Cold Moonlight")
};

/**
 * Project-wide tonemapper choice for the ShaderOverride plugin.
 * Changing these regenerates Shaders/TonemapOverrideConfig.ush and recompiles the LUT shader.
 * Per-volume controls (contrast, toe, shoulder, night vision, bleach bypass, look strength,
 * split-tone strength) live in Post Process Volumes. See the plugin's README.md.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Tonemap Override"))
class UTonemapOverrideSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTonemapOverrideSettings();

	/** Tone curve baked into the color grading LUT. */
	UPROPERTY(Config, EditAnywhere, Category = "Tonemapper")
	ETonemapOverrideMode Tonemapper = ETonemapOverrideMode::AgX;

	/** AgX look. Per-volume strength: Post Process Volume > Blue Correction (0.6 = 100%). */
	UPROPERTY(Config, EditAnywhere, Category = "Tonemapper", meta = (EditCondition = "Tonemapper == ETonemapOverrideMode::AgX"))
	ETonemapOverrideAgxLook AgxLook = ETonemapOverrideAgxLook::Punchy;

	/** Shadow/highlight tint preset. Per-volume strength: Post Process Volume > Expand Gamut (1.0 = 100%). */
	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (EditCondition = "Tonemapper != ETonemapOverrideMode::EngineDefault"))
	ETonemapOverrideSplitTone SplitTone = ETonemapOverrideSplitTone::None;

	/**
	 * 0 = per-channel curve: very bright colors bleach to white.
	 * 100 = hue-preserving: fire and flashlight oranges keep their hue but never reach white.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", EditCondition = "Tonemapper != ETonemapOverrideMode::EngineDefault"))
	int32 HuePreservationPercent = 0;

	/** Writes the shader config header if it differs from these settings. Returns true if rewritten. */
	bool ApplyToShaders(bool bRecompile) const;

	FString BuildConfigHeader() const;
	static FString GetConfigHeaderPath();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif
};
