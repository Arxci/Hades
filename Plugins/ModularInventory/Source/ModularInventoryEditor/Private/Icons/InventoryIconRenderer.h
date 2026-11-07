#pragma once

#include "CoreMinimal.h"

#include "PreviewScene.h"
#include "UObject/StrongObjectPtr.h"

class UDirectionalLightComponent;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
struct FInventoryIconFraming;


/**
 * A small photo studio for item icons: one static mesh, the lights from the editor settings, and a camera that keeps the background transparent.
 * The camera looks along X with Z up. The mesh is turned by the framing around the center of its bounds.
 */
class FInventoryIconRenderer
{
public:
	FInventoryIconRenderer();

	/** Waits until the mesh is built and its textures are fully loaded, so the picture is sharp. */
	void SetMesh(UStaticMesh* Mesh);
	UStaticMesh* GetMesh() const;

	/** sRGB pixels with straight alpha, row by row. Empty when there's no mesh. */
	TArray<FColor> Render(const FInventoryIconFraming& Framing, FIntPoint ImageSize);

private:
	void ApplyLighting();

	/** Linear HDR pixels. Shift moves the camera sideways (Y) and up (Z); HalfWidthTangent sets the horizontal field of view. */
	TArray<FLinearColor> Capture(FIntPoint Size, double Distance, const FVector2D& Shift, double HalfWidthTangent);

	FPreviewScene PreviewScene;

	// The preview scene keeps the components it holds alive.
	UStaticMeshComponent* MeshComponent = nullptr;
	USceneCaptureComponent2D* CaptureComponent = nullptr;
	TArray<UDirectionalLightComponent*> Lights;

	TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
};
