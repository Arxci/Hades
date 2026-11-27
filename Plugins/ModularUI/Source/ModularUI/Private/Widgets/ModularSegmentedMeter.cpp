#include "Widgets/ModularSegmentedMeter.h"

#include "Rendering/DrawElements.h"

#define LOCTEXT_NAMESPACE "ModularSegmentedMeter"

void SModularSegmentedMeter::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
}

void SModularSegmentedMeter::SetStyle(const FStyle& InStyle)
{
	Style = InStyle;
	Style.SegmentCount = FMath::Max(1, Style.SegmentCount);
	Style.SegmentSpacing = FMath::Max(0.0f, Style.SegmentSpacing);

	if (Style.TrailDrainRate <= 0.0f)
	{
		TrailPercent = Percent;
	}

	Invalidate(EInvalidateWidgetReason::Layout);
}

void SModularSegmentedMeter::SetPercent(float InPercent, bool bAnimate)
{
	const float NewPercent = FMath::Clamp(InPercent, 0.0f, 1.0f);
	const float OldPercent = Percent;
	Percent = NewPercent;

	if (!bAnimate || Style.TrailDrainRate <= 0.0f)
	{
		TrailPercent = Percent;
	}
	else if (Percent < OldPercent)
	{
		// A trail already showing grows to where the meter was, and each new loss waits again before draining, so a run of hits reads as one.
		TrailPercent = FMath::Max(TrailPercent, OldPercent);
		TrailWait = Style.TrailDelay;
	}
	else
	{
		// A gain covers as much of the trail as it reaches.
		TrailPercent = FMath::Max(TrailPercent, Percent);
	}

	const TSharedPtr<FActiveTimerHandle> Timer = TrailTimer.Pin();
	if (TrailPercent > Percent)
	{
		if (!Timer.IsValid())
		{
			TrailTimer = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SModularSegmentedMeter::UpdateTrail));
		}
	}
	else if (Timer.IsValid())
	{
		UnRegisterActiveTimer(Timer.ToSharedRef());
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

EActiveTimerReturnType SModularSegmentedMeter::UpdateTrail(double InCurrentTime, float InDeltaTime)
{
	if (TrailWait > 0.0f)
	{
		TrailWait -= InDeltaTime;
		return EActiveTimerReturnType::Continue;
	}

	TrailPercent = FMath::Max(Percent, TrailPercent - Style.TrailDrainRate * InDeltaTime);
	Invalidate(EInvalidateWidgetReason::Paint);

	return TrailPercent > Percent ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

bool SModularSegmentedMeter::IsVertical() const
{
	return Style.FillFrom == EModularMeterFill::BottomToTop || Style.FillFrom == EModularMeterFill::TopToBottom;
}

FVector2D SModularSegmentedMeter::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const FVector2D SegmentSize = Style.SegmentBrush ? FVector2D(Style.SegmentBrush->GetImageSize()) : FVector2D(8.0, 8.0);
	const double Gaps = Style.SegmentSpacing * (Style.SegmentCount - 1);

	return IsVertical()
		? FVector2D(SegmentSize.X, SegmentSize.Y * Style.SegmentCount + Gaps)
		: FVector2D(SegmentSize.X * Style.SegmentCount + Gaps, SegmentSize.Y);
}

int32 SModularSegmentedMeter::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* Brush = Style.SegmentBrush;
	if (!Brush || Brush->DrawAs == ESlateBrushDrawType::NoDrawType)
	{
		return LayerId;
	}

	const bool bVertical = IsVertical();
	const bool bFromFarEnd = Style.FillFrom == EModularMeterFill::RightToLeft || Style.FillFrom == EModularMeterFill::BottomToTop;
	const FVector2f Size = AllottedGeometry.GetLocalSize();
	const float Length = bVertical ? Size.Y : Size.X;
	const float Across = bVertical ? Size.X : Size.Y;
	const int32 Count = Style.SegmentCount;

	// Segments too thin to tell apart lose their gaps before they lose their size.
	float Spacing = Style.SegmentSpacing;
	float SegmentLength = (Length - Spacing * (Count - 1)) / Count;
	if (SegmentLength < 1.0f)
	{
		Spacing = 0.0f;
		SegmentLength = Length / Count;
	}
	if (SegmentLength <= 0.0f || Across <= 0.0f)
	{
		return LayerId;
	}

	/** How much of segment Index a fill reaching Fraction of the whole meter lights, from 0 to 1. */
	auto GetLit = [this, Count](float Fraction, int32 Index)
	{
		if (Style.bWholeSegments)
		{
			return Index < FMath::CeilToInt(Fraction * Count - UE_KINDA_SMALL_NUMBER) ? 1.0f : 0.0f;
		}
		return FMath::Clamp(Fraction * Count - Index, 0.0f, 1.0f);
	};

	const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint() * Brush->GetTint(InWidgetStyle);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Segment 0 lights first, at the end the meter fills from.
		const float Start = bFromFarEnd ? Length - SegmentLength - Index * (SegmentLength + Spacing) : Index * (SegmentLength + Spacing);
		const FVector2f SegmentOffset = bVertical ? FVector2f(0.0f, Start) : FVector2f(Start, 0.0f);
		const FVector2f SegmentSize = bVertical ? FVector2f(Across, SegmentLength) : FVector2f(SegmentLength, Across);
		const FPaintGeometry SegmentGeometry = AllottedGeometry.ToPaintGeometry(SegmentSize, FSlateLayoutTransform(SegmentOffset));

		/** Draws the part of the segment from From to To, as fractions of it along the fill, cut from the whole segment so a texture keeps its shape. */
		auto DrawPart = [&](float From, float To, const FLinearColor& Color)
		{
			if (To - From <= UE_KINDA_SMALL_NUMBER || Color.A <= 0.0f)
			{
				return;
			}

			const FLinearColor PartColor = Tint * Color;
			if (From <= 0.0f && To >= 1.0f)
			{
				FSlateDrawElement::MakeBox(OutDrawElements, LayerId, SegmentGeometry, Brush, DrawEffects, PartColor);
				return;
			}

			// Measured from the end the segment fills from, which is its far end when the meter fills from its far end.
			const float PartStart = bFromFarEnd ? Start + SegmentLength * (1.0f - To) : Start + SegmentLength * From;
			const float PartLength = SegmentLength * (To - From);
			const FGeometry PartGeometry = bVertical
				? AllottedGeometry.MakeChild(FVector2f(Across, PartLength), FSlateLayoutTransform(FVector2f(0.0f, PartStart)))
				: AllottedGeometry.MakeChild(FVector2f(PartLength, Across), FSlateLayoutTransform(FVector2f(PartStart, 0.0f)));

			OutDrawElements.PushClip(FSlateClippingZone(PartGeometry));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, SegmentGeometry, Brush, DrawEffects, PartColor);
			OutDrawElements.PopClip();
		};

		const float Lit = GetLit(Percent, Index);
		const float Trail = FMath::Max(Lit, GetLit(TrailPercent, Index));

		DrawPart(0.0f, Lit, Style.FillColor);
		DrawPart(Lit, Trail, Style.TrailColor);
		DrawPart(Trail, 1.0f, Style.EmptyColor);
	}

	return LayerId;
}


UModularSegmentedMeter::UModularSegmentedMeter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Small, slightly rounded ticks, which a texture of your own can replace.
	SegmentBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	SegmentBrush.ImageSize = FVector2D(14.0, 6.0);
	SegmentBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	SegmentBrush.OutlineSettings.CornerRadii = FVector4(1.0, 1.0, 1.0, 1.0);
	SegmentBrush.OutlineSettings.Width = 0.0f;
}

#if WITH_EDITOR
const FText UModularSegmentedMeter::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Modular UI");
}
#endif

TSharedRef<SWidget> UModularSegmentedMeter::RebuildWidget()
{
	MyMeter = SNew(SModularSegmentedMeter);
	return MyMeter.ToSharedRef();
}

void UModularSegmentedMeter::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (MyMeter)
	{
		MyMeter->SetStyle(MakeStyle());

		// Kept as is when it already matches, so syncing again doesn't cut a trail short.
		if (MyMeter->GetPercent() != Percent)
		{
			MyMeter->SetPercent(Percent, false);
		}
	}
}

void UModularSegmentedMeter::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMeter.Reset();
}

SModularSegmentedMeter::FStyle UModularSegmentedMeter::MakeStyle() const
{
	SModularSegmentedMeter::FStyle Style;
	Style.SegmentCount = SegmentCount;
	Style.SegmentSpacing = SegmentSpacing;
	Style.FillFrom = FillFrom;
	Style.bWholeSegments = bWholeSegments;
	Style.SegmentBrush = &SegmentBrush;
	Style.FillColor = FillColor;
	Style.EmptyColor = EmptyColor;
	Style.TrailColor = TrailColor;
	Style.TrailDelay = TrailDelay;
	Style.TrailDrainRate = TrailDrainRate;
	return Style;
}

void UModularSegmentedMeter::SetPercent(float InPercent)
{
	Percent = FMath::Clamp(InPercent, 0.0f, 1.0f);
	if (MyMeter)
	{
		MyMeter->SetPercent(Percent, true);
	}
}

void UModularSegmentedMeter::SetPercentImmediately(float InPercent)
{
	Percent = FMath::Clamp(InPercent, 0.0f, 1.0f);
	if (MyMeter)
	{
		MyMeter->SetPercent(Percent, false);
	}
}

void UModularSegmentedMeter::SetFillColor(FLinearColor InFillColor)
{
	FillColor = InFillColor;
	if (MyMeter)
	{
		MyMeter->SetStyle(MakeStyle());
	}
}

void UModularSegmentedMeter::SetSegmentCount(int32 InSegmentCount)
{
	SegmentCount = FMath::Clamp(InSegmentCount, 1, 100);
	if (MyMeter)
	{
		MyMeter->SetStyle(MakeStyle());
	}
}

#undef LOCTEXT_NAMESPACE
