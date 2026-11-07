#include "InventoryIconThumbnailRenderer.h"

#include "Engine/Texture2D.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"

UTexture2D* UInventoryIconThumbnailRenderer::FindIcon(UObject* Object)
{
	const UInventoryItemCosmetics* Cosmetics = Cast<UInventoryItemCosmetics>(Object);
	if (const UInventoryItemDefinition* Definition = Cast<UInventoryItemDefinition>(Object))
	{
		Cosmetics = Definition->Cosmetics;
	}
	return Cosmetics ? Cosmetics->Icon.LoadSynchronous() : nullptr;
}

bool UInventoryIconThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return FindIcon(Object) != nullptr;
}

void UInventoryIconThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	Super::GetThumbnailSize(FindIcon(Object), Zoom, OutWidth, OutHeight);
}

void UInventoryIconThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	if (UTexture2D* Icon = FindIcon(Object))
	{
		Super::Draw(Icon, X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
	}
}
