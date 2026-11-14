#include "Widgets/InventoryStatWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UInventoryStatWidget::SetStat(const FInventoryStatDisplay& Display, float Value, float Max, bool bCompare, float ComparedValue)
{
	FNumberFormattingOptions NumberFormat;
	NumberFormat.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(Display.FractionalDigits);

	if (LabelText)
	{
		LabelText->SetText(Display.Label);
	}
	if (ValueText)
	{
		const FText ValueAsText = FText::AsNumber(Value, &NumberFormat);
		ValueText->SetText(Max > 0.0f ? FText::FormatNamed(MaxFormat, TEXT("Value"), ValueAsText, TEXT("Max"), FText::AsNumber(Max, &NumberFormat)) : ValueAsText);
	}
	if (Bar)
	{
		Bar->SetVisibility(Max > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Bar->SetPercent(Max > 0.0f ? FMath::Clamp(Value / Max, 0.0f, 1.0f) : 0.0f);
	}

	// A difference too small to show at this many digits would read as +0.
	const float Difference = bCompare ? Value - ComparedValue : 0.0f;
	const bool bDiffers = FMath::Abs(Difference) >= 0.5f * FMath::Pow(10.0f, -static_cast<float>(Display.FractionalDigits));
	if (DifferenceText)
	{
		DifferenceText->SetVisibility(bDiffers ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bDiffers)
		{
			FNumberFormattingOptions DifferenceFormat = NumberFormat;
			DifferenceFormat.SetAlwaysSign(true);
			const bool bBetter = (Difference > 0.0f) != Display.bLowerIsBetter;
			DifferenceText->SetText(FText::AsNumber(Difference, &DifferenceFormat));
			DifferenceText->SetColorAndOpacity(FSlateColor(bBetter ? BetterColor : WorseColor));
		}
	}

	OnStatChanged(Display, Value, Max, bCompare, Difference);
}
