#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "InventoryGridLinesWidget.generated.h"


/**
 * Thin lines between a grid's cells, running Overhang pixels past its outer edges. With a gap between cells, the lines run
 * along the middle of the gaps, and half a gap outside the outer cells. An Inventory Grid Widget with Draw Grid Lines on makes
 * one under its cells and keeps it the size of the grid, so you don't place this yourself.
 */
UCLASS()
class MODULARINVENTORYUI_API UInventoryGridLinesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGrid(FIntPoint InGridSize, float InCellSize, float InCellGap);
	void SetLineStyle(const FLinearColor& InColor, float InThickness, float InOverhang);

protected:
	//~ Begin UUserWidget Interface
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//~ End UUserWidget Interface

private:
	FIntPoint GridSize = FIntPoint::ZeroValue;
	float CellSize = 0.0f;
	float CellGap = 0.0f;
	FLinearColor Color = FLinearColor::White;
	float Thickness = 1.0f;
	float Overhang = 0.0f;
};
