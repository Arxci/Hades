#pragma once

#include "CoreMinimal.h"

#include "ThumbnailRendering/TextureThumbnailRenderer.h"

#include "InventoryIconThumbnailRenderer.generated.h"

class UTexture2D;


/** Shows the item's icon as the Content Browser thumbnail of item definitions and cosmetics. Without an icon, the default thumbnail stays. */
UCLASS()
class UInventoryIconThumbnailRenderer : public UTextureThumbnailRenderer
{
	GENERATED_BODY()

public:
	//~ Begin UThumbnailRenderer Interface
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	//~ End UThumbnailRenderer Interface

private:
	static UTexture2D* FindIcon(UObject* Object);
};
