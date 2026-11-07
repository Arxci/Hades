#pragma once

#include "CoreMinimal.h"

#include "Engine/DeveloperSettings.h"

#include "InventoryEditorSettings.generated.h"


USTRUCT()
struct FInventoryIconLight
{
	GENERATED_BODY()

	/** The way the light shines, seen from the icon camera: Yaw 0 shines away from the camera, Yaw 90 to the right, Pitch -90 straight down. */
	UPROPERTY(EditAnywhere, Category = "Light")
	FRotator Direction = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Light", meta = (ClampMin = "0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Light")
	FLinearColor Color = FLinearColor::White;
};


/** How the Modular Inventory editor tools name assets and render icons. Saved to DefaultEditor.ini, so the whole team shares them. */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Modular Inventory"))
class UInventoryEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UInventoryEditorSettings();

	/** Opens this page in Project Settings. */
	static void Open();

	/** The name without any of the prefixes and suffixes below: SM_Lantern, T_Lantern_Icon and DA_Lantern_Cosmetics all give Lantern. */
	FString GetBaseName(const FString& AssetName) const;
	FString MakeItemName(const FString& BaseName) const { return ItemPrefix + BaseName; }
	FString MakeCosmeticsName(const FString& BaseName) const { return CosmeticsPrefix + BaseName + CosmeticsSuffix; }
	FString MakeIconName(const FString& BaseName) const { return IconPrefix + BaseName + IconSuffix; }

	UPROPERTY(config, EditAnywhere, Category = "Naming")
	FString ItemPrefix;

	UPROPERTY(config, EditAnywhere, Category = "Naming")
	FString CosmeticsPrefix;

	UPROPERTY(config, EditAnywhere, Category = "Naming")
	FString CosmeticsSuffix;

	UPROPERTY(config, EditAnywhere, Category = "Naming")
	FString IconPrefix;

	UPROPERTY(config, EditAnywhere, Category = "Naming")
	FString IconSuffix;

	/** Also dropped when naming items after meshes and textures. */
	UPROPERTY(config, EditAnywhere, Category = "Naming")
	TArray<FString> SourcePrefixes;

	/** Icons are this many pixels per grid cell: a 1x3 item gets a 128x384 icon. */
	UPROPERTY(config, EditAnywhere, Category = "Icons", meta = (ClampMin = "16", ClampMax = "1024"))
	int32 PixelsPerCell = 128;

	/** Renders this many times larger and shrinks the result, for smooth edges. */
	UPROPERTY(config, EditAnywhere, Category = "Icons", meta = (ClampMin = "1", ClampMax = "4"))
	int32 Supersampling = 2;

	/** Lower looks flatter, closer to an orthographic camera. */
	UPROPERTY(config, EditAnywhere, Category = "Icons", meta = (ClampMin = "1", ClampMax = "90", Units = "Degrees"))
	float FieldOfView = 15.0f;

	/** Empty space kept around the mesh, as a fraction of the icon. */
	UPROPERTY(config, EditAnywhere, Category = "Icons", meta = (ClampMin = "0", ClampMax = "0.4"))
	float Padding = 0.06f;

	/** How new icons turn the mesh. The camera looks along X with Z up. Long meshes are also turned to lie along the long side of their cells. */
	UPROPERTY(config, EditAnywhere, Category = "Icons")
	FRotator DefaultRotation;

	/** Where new icons are saved: a folder next to the cosmetics, such as Icons, or a full path, such as /Game/UI/Icons. Empty saves them beside the cosmetics. */
	UPROPERTY(config, EditAnywhere, Category = "Icons")
	FString IconFolder;

	/** Create Inventory Item renders an icon for items made from a mesh alone. */
	UPROPERTY(config, EditAnywhere, Category = "Icons")
	bool bGenerateIconsForNewItems = true;

	UPROPERTY(config, EditAnywhere, Category = "Icons|Lighting", meta = (ClampMin = "0"))
	float Exposure = 1.0f;

	/** Soft light from every direction, like an overcast sky. */
	UPROPERTY(config, EditAnywhere, Category = "Icons|Lighting", meta = (ClampMin = "0"))
	float AmbientIntensity = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "Icons|Lighting")
	TArray<FInventoryIconLight> Lights;
};
