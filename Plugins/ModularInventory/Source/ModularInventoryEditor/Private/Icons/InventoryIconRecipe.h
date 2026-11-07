#pragma once

#include "CoreMinimal.h"

#include "Engine/AssetUserData.h"

#include "InventoryIconRecipe.generated.h"


/** How the mesh sits in an item icon. The mesh is always centered and fitted first; these adjust the result. */
USTRUCT()
struct FInventoryIconFraming
{
	GENERATED_BODY()

	bool operator==(const FInventoryIconFraming& Other) const
	{
		return Rotation == Other.Rotation && Zoom == Other.Zoom && Offset == Other.Offset;
	}

	UPROPERTY(EditAnywhere, Category = "Framing")
	FRotator Rotation = FRotator::ZeroRotator;

	/** 1 fits the mesh to the icon, 2 shows it twice as large. */
	UPROPERTY(EditAnywhere, Category = "Framing", meta = (ClampMin = "0.1", ClampMax = "10", UIMin = "0.25", UIMax = "4"))
	float Zoom = 1.0f;

	/** Moves the mesh by a fraction of the icon: X to the right, Y down. */
	UPROPERTY(EditAnywhere, Category = "Framing", meta = (UIMin = "-0.5", UIMax = "0.5"))
	FVector2D Offset = FVector2D::ZeroVector;
};


/**
 * Marks a texture as an icon the generator made, and keeps its framing so it can be rendered again with new settings.
 * Textures without one are hand-made and never overwritten. Editor only: it isn't cooked.
 */
UCLASS()
class UInventoryIconRecipe : public UAssetUserData
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual bool IsEditorOnly() const override { return true; }
	//~ End UObject Interface

	UPROPERTY(EditAnywhere, Category = "Icon", meta = (ShowOnlyInnerProperties))
	FInventoryIconFraming Framing;
};
