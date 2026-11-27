#pragma once

#include "CoreMinimal.h"

#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

#include "ModularSegmentedMeter.generated.h"


/** Which end a meter fills from. */
UENUM(BlueprintType)
enum class EModularMeterFill : uint8
{
	LeftToRight,
	RightToLeft,
	BottomToTop,
	TopToBottom
};


/** The Slate widget behind Modular Segmented Meter: a row of segments, lit up to a fraction, with a trail behind what was lost. */
class MODULARUI_API SModularSegmentedMeter : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SModularSegmentedMeter) {}
	SLATE_END_ARGS()

	/** How it looks. The brush is kept by whoever sets it, such as the UMG widget holding this. */
	struct FStyle
	{
		int32 SegmentCount = 10;
		float SegmentSpacing = 2.0f;
		EModularMeterFill FillFrom = EModularMeterFill::LeftToRight;
		bool bWholeSegments = false;
		const FSlateBrush* SegmentBrush = nullptr;
		FLinearColor FillColor = FLinearColor::White;
		FLinearColor EmptyColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.12f);
		FLinearColor TrailColor = FLinearColor(0.55f, 0.07f, 0.05f, 0.9f);
		float TrailDelay = 0.35f;
		float TrailDrainRate = 0.6f;
	};

	void Construct(const FArguments& InArgs);

	void SetStyle(const FStyle& InStyle);

	/** Lights the meter up to Percent, from 0 to 1. With Animate, a loss leaves a trail that drains away after Trail Delay. */
	void SetPercent(float InPercent, bool bAnimate);

	float GetPercent() const { return Percent; }

	//~ Begin SWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	//~ End SWidget Interface

private:
	EActiveTimerReturnType UpdateTrail(double InCurrentTime, float InDeltaTime);
	bool IsVertical() const;

	FStyle Style;
	float Percent = 1.0f;

	/** Where the trail reaches, at or past Percent, and how long it waits before it drains. */
	float TrailPercent = 1.0f;
	float TrailWait = 0.0f;
	TWeakPtr<FActiveTimerHandle> TrailTimer;
};


/**
 * A meter of separate segments, lit up to a fraction, such as health, a battery's charge, stamina or rounds, read at a glance and less like a
 * placeholder than a plain progress bar. A loss leaves a trail, in Trail Color, that drains away after Trail Delay, so the player sees how much
 * was lost, as fighting games and survival games show damage. Animated in real time, so it moves while the game is paused, such as behind
 * a menu.
 * Each segment draws Segment Brush, tinted: give it a rounded box, or a texture of your own, such as a worn tick mark. The meter fills the
 * space it's given; Segment Brush's Image Size is only how big it wants to be.
 */
UCLASS(meta = (DisplayName = "Modular Segmented Meter"))
class MODULARUI_API UModularSegmentedMeter : public UWidget
{
	GENERATED_BODY()

public:
	UModularSegmentedMeter(const FObjectInitializer& ObjectInitializer);

	/** Lights the meter up to Percent, from 0 to 1. A loss leaves a trail that drains away. */
	UFUNCTION(BlueprintCallable, Category = "Meter")
	void SetPercent(float InPercent);

	/** Sets the meter to Percent at once, without a trail, such as as it first shows. */
	UFUNCTION(BlueprintCallable, Category = "Meter")
	void SetPercentImmediately(float InPercent);

	UFUNCTION(BlueprintPure, Category = "Meter")
	float GetPercent() const { return Percent; }

	UFUNCTION(BlueprintCallable, Category = "Meter")
	void SetFillColor(FLinearColor InFillColor);

	UFUNCTION(BlueprintPure, Category = "Meter")
	FLinearColor GetFillColor() const { return FillColor; }

	UFUNCTION(BlueprintCallable, Category = "Meter")
	void SetSegmentCount(int32 InSegmentCount);

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	/** How full it is, from 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float Percent = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter", meta = (ClampMin = "1", ClampMax = "100"))
	int32 SegmentCount = 10;

	/** Space between segments, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter", meta = (ClampMin = "0"))
	float SegmentSpacing = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter")
	EModularMeterFill FillFrom = EModularMeterFill::LeftToRight;

	/** Segments light whole, the last one partly lit counting as lit, as a ticking gauge does. Off, the last lit segment fills partway. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meter")
	bool bWholeSegments = false;

	/** What each segment draws, tinted by the colors below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FSlateBrush SegmentBrush;

	/** The lit segments. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor FillColor = FLinearColor::White;

	/** The segments not lit, such as a faint version of Fill Color. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor EmptyColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.12f);

	/** What was just lost, between the meter and where it was, until it drains away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Trail")
	FLinearColor TrailColor = FLinearColor(0.55f, 0.07f, 0.05f, 0.9f);

	/** How long the trail waits before it drains, in seconds of real time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Trail", meta = (ClampMin = "0", Units = "s"))
	float TrailDelay = 0.35f;

	/** How fast the trail drains, as a fraction of the whole meter per second. 0 shows no trail. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Trail", meta = (ClampMin = "0"))
	float TrailDrainRate = 0.6f;

private:
	SModularSegmentedMeter::FStyle MakeStyle() const;

	TSharedPtr<SModularSegmentedMeter> MyMeter;
};
