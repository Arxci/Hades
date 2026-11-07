#include "Icons/InventoryIconRenderer.h"

#include "AssetCompilingManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "ThumbnailRendering/ThumbnailManager.h"

#include "Icons/InventoryIconRecipe.h"
#include "InventoryEditorSettings.h"

namespace
{
	/** The box around the pixels the mesh covers, in frame coordinates from 0 to 1: X to the right, Y down. Invalid when nothing rendered. */
	FBox2D FindSilhouette(const TArray<FLinearColor>& Pixels, FIntPoint Size)
	{
		FBox2D Silhouette(ForceInit);
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				// Scene color captures store inverse opacity in alpha.
				if (Pixels[Y * Size.X + X].A < 0.99f)
				{
					Silhouette += FVector2D(X, Y);
					Silhouette += FVector2D(X + 1, Y + 1);
				}
			}
		}

		if (Silhouette.bIsValid)
		{
			Silhouette.Min /= FVector2D(Size);
			Silhouette.Max /= FVector2D(Size);
		}
		return Silhouette;
	}

	/** Krzysztof Narkowicz's fit of the ACES filmic curve, close to Unreal's default tonemapper. */
	float ToneMap(float Value)
	{
		return FMath::Clamp((Value * (2.51f * Value + 0.03f)) / (Value * (2.43f * Value + 0.59f) + 0.14f), 0.0f, 1.0f);
	}

	/** Averages each Supersampling x Supersampling block of premultiplied HDR samples into one tone-mapped sRGB pixel with straight alpha. */
	TArray<FColor> ResolveSamples(const TArray<FLinearColor>& Samples, FIntPoint ImageSize, int32 Supersampling, float Exposure)
	{
		const int32 RowLength = ImageSize.X * Supersampling;
		const float SamplesPerPixel = static_cast<float>(Supersampling * Supersampling);

		TArray<FColor> Pixels;
		Pixels.Reserve(ImageSize.X * ImageSize.Y);
		for (int32 Y = 0; Y < ImageSize.Y; ++Y)
		{
			for (int32 X = 0; X < ImageSize.X; ++X)
			{
				FLinearColor Sum(0.0f, 0.0f, 0.0f, 0.0f);
				for (int32 SampleY = 0; SampleY < Supersampling; ++SampleY)
				{
					for (int32 SampleX = 0; SampleX < Supersampling; ++SampleX)
					{
						const FLinearColor& Sample = Samples[(Y * Supersampling + SampleY) * RowLength + X * Supersampling + SampleX];
						Sum += FLinearColor(Sample.R, Sample.G, Sample.B, 1.0f - Sample.A);
					}
				}

				const FLinearColor Color = Sum.A > 0.0f ? FLinearColor(Sum.R, Sum.G, Sum.B) * (Exposure / Sum.A) : FLinearColor::Black;
				FColor Pixel = FLinearColor(ToneMap(Color.R), ToneMap(Color.G), ToneMap(Color.B)).ToFColor(true);
				Pixel.A = static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(Sum.A / SamplesPerPixel, 0.0f, 1.0f) * 255.0f));
				Pixels.Add(Pixel);
			}
		}
		return Pixels;
	}
}

FInventoryIconRenderer::FInventoryIconRenderer()
	: PreviewScene(FPreviewScene::ConstructionValues()
		.SetLightBrightness(0.0f)
		.SetCreatePhysicsScene(false)
		.SetTransactional(false))
{
	PreviewScene.SetSkyCubemap(UThumbnailManager::Get().AmbientCubemap);
	PreviewScene.UpdateCaptureContents();

	MeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewScene.AddComponent(MeshComponent, FTransform::Identity);

	RenderTarget.Reset(NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient));
	RenderTarget->RenderTargetFormat = RTF_RGBA16f;
	RenderTarget->InitAutoFormat(64, 64);

	CaptureComponent = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	CaptureComponent->CaptureSource = SCS_SceneColorHDR;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->ShowOnlyComponent(MeshComponent);
	CaptureComponent->TextureTarget = RenderTarget.Get();

	// Lumen needs several frames to settle, and every icon is a single frame.
	FPostProcessSettings& PostProcess = CaptureComponent->PostProcessSettings;
	PostProcess.bOverride_DynamicGlobalIlluminationMethod = true;
	PostProcess.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PostProcess.bOverride_ReflectionMethod = true;
	PostProcess.ReflectionMethod = EReflectionMethod::None;

	PreviewScene.AddComponent(CaptureComponent, FTransform::Identity);
}

void FInventoryIconRenderer::SetMesh(UStaticMesh* Mesh)
{
	MeshComponent->SetStaticMesh(Mesh);

	// Meshes and shaders build in the background, and textures nobody has looked at yet are streamed in at low resolution.
	FAssetCompilingManager::Get().FinishAllCompilation();
	TArray<UTexture*> Textures;
	MeshComponent->GetUsedTextures(Textures, EMaterialQualityLevel::Num);
	for (UTexture* Texture : Textures)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
			Texture2D->SetForceMipLevelsToBeResident(30.0f);
			Texture2D->WaitForStreaming();
		}
	}
}

UStaticMesh* FInventoryIconRenderer::GetMesh() const
{
	return MeshComponent->GetStaticMesh();
}

TArray<FColor> FInventoryIconRenderer::Render(const FInventoryIconFraming& Framing, FIntPoint ImageSize)
{
	const UStaticMesh* Mesh = GetMesh();
	if (!Mesh || ImageSize.X < 1 || ImageSize.Y < 1)
	{
		return {};
	}

	const UInventoryEditorSettings* Settings = GetDefault<UInventoryEditorSettings>();
	ApplyLighting();

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FQuat Rotation(Framing.Rotation);
	MeshComponent->SetWorldLocationAndRotation(-Rotation.RotateVector(Bounds.Origin), Rotation);

	const int32 Supersampling = FMath::Clamp(Settings->Supersampling, 1, 4);
	const FIntPoint RenderSize = ImageSize * Supersampling;
	const double Aspect = static_cast<double>(ImageSize.X) / ImageSize.Y;

	// First far enough back that the bounding sphere fits the narrower side, then narrowed to the silhouette that actually rendered.
	double Tangent = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Settings->FieldOfView)) * 0.5);
	const double NarrowTangent = FMath::Min(Tangent, Tangent / Aspect);
	const double Distance = FMath::Max(Bounds.SphereRadius, 1.0) * FMath::Sqrt(1.0 + 1.0 / FMath::Square(NarrowTangent));
	FVector2D Shift = FVector2D::ZeroVector;

	const TArray<FLinearColor> FirstPass = Capture(RenderSize, Distance, Shift, Tangent);
	if (FirstPass.IsEmpty())
	{
		return {};
	}

	const FBox2D Silhouette = FindSilhouette(FirstPass, RenderSize);
	if (Silhouette.bIsValid)
	{
		const FVector2D FirstFrameSize(2.0 * Distance * Tangent, 2.0 * Distance * Tangent / Aspect);
		const FVector2D SilhouetteCenter = (Silhouette.GetCenter() - FVector2D(0.5, 0.5)) * FirstFrameSize;
		const FVector2D SilhouetteSize = Silhouette.GetSize() * FirstFrameSize;
		Shift += FVector2D(SilhouetteCenter.X, -SilhouetteCenter.Y);
		Tangent = FMath::Max(SilhouetteSize.X, SilhouetteSize.Y * Aspect) / (2.0 * Distance);
	}

	Tangent *= (1.0 + 2.0 * Settings->Padding) / FMath::Max(Framing.Zoom, 0.01f);
	const FVector2D FrameSize(2.0 * Distance * Tangent, 2.0 * Distance * Tangent / Aspect);
	Shift -= FVector2D(Framing.Offset.X, -Framing.Offset.Y) * FrameSize;

	const TArray<FLinearColor> FinalPass = Capture(RenderSize, Distance, Shift, Tangent);
	return FinalPass.IsEmpty() ? TArray<FColor>() : ResolveSamples(FinalPass, ImageSize, Supersampling, Settings->Exposure);
}

void FInventoryIconRenderer::ApplyLighting()
{
	const UInventoryEditorSettings* Settings = GetDefault<UInventoryEditorSettings>();
	PreviewScene.SetSkyBrightness(Settings->AmbientIntensity);

	while (Lights.Num() < Settings->Lights.Num())
	{
		UDirectionalLightComponent* Light = NewObject<UDirectionalLightComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);
		PreviewScene.AddComponent(Light, FTransform::Identity);
		Lights.Add(Light);
	}

	for (int32 Index = 0; Index < Lights.Num(); ++Index)
	{
		const FInventoryIconLight* LightSettings = Settings->Lights.IsValidIndex(Index) ? &Settings->Lights[Index] : nullptr;
		Lights[Index]->SetIntensity(LightSettings ? LightSettings->Intensity : 0.0f);
		if (LightSettings)
		{
			Lights[Index]->SetWorldRotation(LightSettings->Direction);
			Lights[Index]->SetLightColor(LightSettings->Color);
		}
	}
}

TArray<FLinearColor> FInventoryIconRenderer::Capture(FIntPoint Size, double Distance, const FVector2D& Shift, double HalfWidthTangent)
{
	if (RenderTarget->SizeX != Size.X || RenderTarget->SizeY != Size.Y)
	{
		RenderTarget->InitAutoFormat(Size.X, Size.Y);
	}

	CaptureComponent->SetWorldLocation(FVector(-Distance, Shift.X, Shift.Y));
	CaptureComponent->FOVAngle = static_cast<float>(FMath::RadiansToDegrees(2.0 * FMath::Atan(HalfWidthTangent)));
	CaptureComponent->CaptureScene();

	TArray<FLinearColor> Pixels;
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadLinearColorPixels(Pixels) || Pixels.Num() != Size.X * Size.Y)
	{
		Pixels.Reset();
	}
	return Pixels;
}
