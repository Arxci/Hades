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

	/** How far the icon shadow reaches past the mesh, in icon pixels: the room it needs around it. 0 without a shadow. */
	float GetShadowReach() const;

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

	/**
	 * Renaming or moving an item definition renames or moves its cosmetics and icon with it, when they're named after it and sit where the
	 * tools put them: the cosmetics beside the item, the icon beside the cosmetics or in Icon Folder below them. Others are left alone.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Naming")
	bool bRenameItemAssets = true;

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

	/**
	 * Where new icons are saved: a folder next to the cosmetics, such as Icons, or a full path, such as /Game/UI/Icons. Empty saves them
	 * beside the cosmetics. Icons that already exist stay where they are when this changes.
	 */
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

	/**
	 * Bakes a soft shadow of the mesh into every generated icon, under it, so items stand out on any tile. Icons get a little more room
	 * around the mesh for it. Generate the icons again after changing these.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Icons|Shadow")
	bool bDrawShadow = false;

	/** The shadow's color. Its alpha is how dark the shadow is where it's thickest. */
	UPROPERTY(config, EditAnywhere, Category = "Icons|Shadow", meta = (EditCondition = "bDrawShadow"))
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	/** How far the shadow fades out, in icon pixels. 0 gives a hard edge. */
	UPROPERTY(config, EditAnywhere, Category = "Icons|Shadow", meta = (EditCondition = "bDrawShadow", ClampMin = "0", ClampMax = "64"))
	float ShadowSoftness = 10.0f;

	/** Grows the shadow by this many icon pixels before it's softened, so it hugs the mesh like a dark outline. */
	UPROPERTY(config, EditAnywhere, Category = "Icons|Shadow", meta = (EditCondition = "bDrawShadow", ClampMin = "0", ClampMax = "32"))
	int32 ShadowSpread = 2;

	/** Moves the shadow, in icon pixels: X to the right, Y down. */
	UPROPERTY(config, EditAnywhere, Category = "Icons|Shadow", meta = (EditCondition = "bDrawShadow"))
	FVector2D ShadowOffset = FVector2D(0.0, 2.0);
};
