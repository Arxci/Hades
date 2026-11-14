#include "Widgets/InventoryGridLinesWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"

void UInventoryGridLinesWidget::SetGrid(FIntPoint InGridSize, float InCellSize, float InCellGap)
{
	if (GridSize != InGridSize || CellSize != InCellSize || CellGap != InCellGap)
	{
		GridSize = InGridSize;
		CellSize = InCellSize;
		CellGap = InCellGap;
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

	// The overhang, and the half gap around the outer cells, are drawn outside this widget's own bounds, so the grid's layout never makes room for them.
	FPaintContext Context(AllottedGeometry, MyCullingRect, OutDrawElements, MaxLayer, InWidgetStyle, bParentEnabled);
	const FLinearColor Tint = Color * InWidgetStyle.GetColorAndOpacityTint();
	const double Pitch = CellSize + CellGap;
	const double HalfGap = CellGap * 0.5;
	const double Left = -HalfGap - Overhang;
	const double Top = -HalfGap - Overhang;
	const double Right = GridSize.X * Pitch - HalfGap + Overhang;
	const double Bottom = GridSize.Y * Pitch - HalfGap + Overhang;

	for (int32 Column = 0; Column <= GridSize.X; ++Column)
	{
		const double X = Column * Pitch - HalfGap;
		UWidgetBlueprintLibrary::DrawLine(Context, FVector2D(X, Top), FVector2D(X, Bottom), Tint, true, Thickness);
	}
	for (int32 Row = 0; Row <= GridSize.Y; ++Row)
	{
		const double Y = Row * Pitch - HalfGap;
		UWidgetBlueprintLibrary::DrawLine(Context, FVector2D(Left, Y), FVector2D(Right, Y), Tint, true, Thickness);
	}
	return FMath::Max(MaxLayer, Context.MaxLayer);
}
