#include "Widgets/InventoryGridLinesWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"

void UInventoryGridLinesWidget::SetGrid(FIntPoint InGridSize, float InCellSize)
{
	if (GridSize != InGridSize || CellSize != InCellSize)
	{
		GridSize = InGridSize;
		CellSize = InCellSize;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UInventoryGridLinesWidget::SetLineStyle(const FLinearColor& InColor, float InThickness, float InOverhang)
{
	Color = InColor;
	Thickness = InThickness;
	Overhang = InOverhang;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UInventoryGridLinesWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (GridSize.X <= 0 || GridSize.Y <= 0 || CellSize <= 0.0f || Color.A <= 0.0f)
	{
		return MaxLayer;
	}

	// The overhang is drawn outside this widget's own bounds, so the grid's layout never makes room for it.
	FPaintContext Context(AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayer, InWidgetStyle, bParentEnabled);
	const FLinearColor Tint = Color * InWidgetStyle.GetColorAndOpacityTint();
	const double Width = GridSize.X * CellSize;
	const double Height = GridSize.Y * CellSize;

	for (int32 Column = 0; Column <= GridSize.X; ++Column)
	{
		const double X = Column * CellSize;
		UWidgetBlueprintLibrary::DrawLine(Context, FVector2D(X, -Overhang), FVector2D(X, Height + Overhang), Tint, true, Thickness);
	}
	for (int32 Row = 0; Row <= GridSize.Y; ++Row)
	{
		const double Y = Row * CellSize;
		UWidgetBlueprintLibrary::DrawLine(Context, FVector2D(-Overhang, Y), FVector2D(Width + Overhang, Y), Tint, true, Thickness);
	}
	return FMath::Max(MaxLayer, Context.MaxLayer);
}
