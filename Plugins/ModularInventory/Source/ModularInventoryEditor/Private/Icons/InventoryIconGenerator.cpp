#include "Icons/InventoryIconGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "IAssetTools.h"
#include "Logging/MessageLog.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/UObjectToken.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "Icons/InventoryIconRecipe.h"
#include "Icons/InventoryIconRenderer.h"
#include "InventoryEditorSettings.h"
#include "ModularInventoryEditor.h"

#define LOCTEXT_NAMESPACE "InventoryIconGenerator"

namespace
{
	FIntPoint GetCells(const UInventoryItemDefinition& Item)
	{
		return Item.Size.ComponentMax(FIntPoint(1, 1));
	}

	/** The size of the turned bounding box as the icon camera sees it: screen X is world Y, screen Y is world Z. */
	FVector2D GetProjectedSize(const FBoxSphereBounds& Bounds, const FQuat& Rotation)
	{
		FBox2D Projected(ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Offset = Bounds.BoxExtent * FVector((Corner & 1) ? 1.0 : -1.0, (Corner & 2) ? 1.0 : -1.0, (Corner & 4) ? 1.0 : -1.0);
			const FVector Turned = Rotation.RotateVector(Offset);
			Projected += FVector2D(Turned.Y, Turned.Z);
		}
		return Projected.GetSize();
	}

	UTexture2D* CreateIconTexture(const UInventoryItemDefinition& Item, const UInventoryItemCosmetics& Cosmetics)
	{
		const UInventoryEditorSettings* Settings = GetDefault<UInventoryEditorSettings>();
		FString Folder = FPackageName::GetLongPackagePath(Cosmetics.GetPackage()->GetName());
		if (Settings->IconFolder.StartsWith(TEXT("/")))
		{
			Folder = Settings->IconFolder;
		}
		else if (!Settings->IconFolder.IsEmpty())
		{
			Folder /= Settings->IconFolder;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		FString PackageName;
		FString AssetName;
		AssetTools.CreateUniqueAssetName(Folder / Settings->MakeIconName(Settings->GetBaseName(Item.GetName())), FString(), PackageName, AssetName);

		UTexture2D* Texture = NewObject<UTexture2D>(CreatePackage(*PackageName), *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Texture);
		return Texture;
	}

	void WriteIcon(UTexture2D& Texture, FIntPoint Size, const TArray<FColor>& Pixels, const FInventoryIconFraming& Framing)
	{
		Texture.Source.Init(Size.X, Size.Y, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
		Texture.CompressionSettings = TC_EditorIcon;
		Texture.MipGenSettings = TMGS_NoMipmaps;
		Texture.LODGroup = TEXTUREGROUP_UI;
		Texture.SRGB = true;

		UInventoryIconRecipe* Recipe = InventoryIcons::FindRecipe(&Texture);
		if (!Recipe)
		{
			Recipe = NewObject<UInventoryIconRecipe>(&Texture);
			Texture.AddAssetUserData(Recipe);
		}
		Recipe->Framing = Framing;

		Texture.PostEditChange();
		Texture.MarkPackageDirty();
	}
}

void InventoryIcons::GenerateIcons(TConstArrayView<UInventoryItemDefinition*> Items)
{
	if (Items.IsEmpty())
	{
		return;
	}

	FScopedSlowTask SlowTask(static_cast<float>(Items.Num()), LOCTEXT("Generating", "Generating item icons"));
	SlowTask.MakeDialog(true);

	FInventoryIconRenderer Renderer;
	FMessageLog Log(InventoryEditor::MessageLogName);
	int32 Generated = 0;
	for (UInventoryItemDefinition* Item : Items)
	{
		if (SlowTask.ShouldCancel())
		{
			break;
		}
		SlowTask.EnterProgressFrame(1.0f, FText::FromString(Item->GetName()));

		UTexture2D* Icon = LoadIcon(*Item);
		if (Icon && !FindRecipe(Icon))
		{
			Log.Info()
				->AddToken(FUObjectToken::Create(Item))
				->AddToken(FTextToken::Create(LOCTEXT("HandMade", "keeps its hand-made icon. Use Edit Item Icon to replace it.")));
			continue;
		}

		FText Error;
		if (SaveIcon(*Item, GetFraming(*Item), Renderer, Error))
		{
			++Generated;
		}
		else
		{
			Log.Warning()->AddToken(FUObjectToken::Create(Item))->AddToken(FTextToken::Create(Error));
		}
	}

	InventoryEditor::Notify(FText::Format(LOCTEXT("Generated", "Generated {0} item icons. Save the changed assets to keep them."), FText::AsNumber(Generated)));
	Log.Notify(LOCTEXT("Skipped", "Some items didn't get a new icon."), EMessageSeverity::Info);
}

bool InventoryIcons::SaveIcon(UInventoryItemDefinition& Item, const FInventoryIconFraming& Framing, FInventoryIconRenderer& Renderer, FText& OutError)
{
	UInventoryItemCosmetics* Cosmetics = Item.Cosmetics;
	if (!Cosmetics)
	{
		OutError = LOCTEXT("NoCosmetics", "has no cosmetics, so there's no World Mesh to render.");
		return false;
	}

	UStaticMesh* Mesh = LoadWorldMesh(Item);
	if (!Mesh)
	{
		OutError = LOCTEXT("NoWorldMesh", "has no World Mesh to render. Set one on its cosmetics.");
		return false;
	}

	if (Renderer.GetMesh() != Mesh)
	{
		Renderer.SetMesh(Mesh);
	}

	const FIntPoint Size = GetIconSize(Item);
	const TArray<FColor> Pixels = Renderer.Render(Framing, Size);
	if (Pixels.IsEmpty())
	{
		OutError = LOCTEXT("RenderFailed", "rendered nothing. Check its World Mesh.");
		return false;
	}

	UTexture2D* Icon = LoadIcon(Item);
	if (!FindRecipe(Icon))
	{
		Icon = CreateIconTexture(Item, *Cosmetics);
		Cosmetics->Modify();
		Cosmetics->Icon = Icon;
		Cosmetics->PostEditChange();
	}

	WriteIcon(*Icon, Size, Pixels, Framing);
	return true;
}

FInventoryIconFraming InventoryIcons::GetFraming(const UInventoryItemDefinition& Item)
{
	if (const UInventoryIconRecipe* Recipe = FindRecipe(LoadIcon(Item)))
	{
		return Recipe->Framing;
	}

	const UStaticMesh* Mesh = LoadWorldMesh(Item);
	return Mesh ? MakeDefaultFraming(*Mesh, GetCells(Item)) : FInventoryIconFraming();
}

FInventoryIconFraming InventoryIcons::MakeDefaultFraming(const UStaticMesh& Mesh, FIntPoint Cells)
{
	FQuat Rotation(GetDefault<UInventoryEditorSettings>()->DefaultRotation);

	// A long mesh lies along the long side of its cells.
	const FVector2D Projected = GetProjectedSize(Mesh.GetBounds(), Rotation);
	if (Cells.X != Cells.Y && (Cells.X > Cells.Y) != (Projected.X > Projected.Y))
	{
		Rotation = FQuat(FVector::ForwardVector, UE_HALF_PI) * Rotation;
	}

	FInventoryIconFraming Framing;
	Framing.Rotation = Rotation.Rotator();
	return Framing;
}

UStaticMesh* InventoryIcons::LoadWorldMesh(const UInventoryItemDefinition& Item)
{
	return Item.Cosmetics ? Item.Cosmetics->WorldMesh.LoadSynchronous() : nullptr;
}

UTexture2D* InventoryIcons::LoadIcon(const UInventoryItemDefinition& Item)
{
	return Item.Cosmetics ? Item.Cosmetics->Icon.LoadSynchronous() : nullptr;
}

UInventoryIconRecipe* InventoryIcons::FindRecipe(UTexture2D* Icon)
{
	return Icon ? Cast<UInventoryIconRecipe>(Icon->GetAssetUserDataOfClass(UInventoryIconRecipe::StaticClass())) : nullptr;
}

FIntPoint InventoryIcons::GetIconSize(const UInventoryItemDefinition& Item)
{
	return GetCells(Item) * GetDefault<UInventoryEditorSettings>()->PixelsPerCell;
}

bool InventoryIcons::MatchesCells(const UTexture2D& Icon, FIntPoint Cells)
{
	const double Width = static_cast<double>(Icon.Source.GetSizeX());
	const double Height = static_cast<double>(Icon.Source.GetSizeY());
	if (Width <= 0.0 || Height <= 0.0 || Cells.X < 1 || Cells.Y < 1)
	{
		return true;
	}

	const double IconShape = Width / Height;
	const double CellShape = static_cast<double>(Cells.X) / Cells.Y;
	return FMath::Abs(IconShape / CellShape - 1.0) < 0.02;
}

#undef LOCTEXT_NAMESPACE
