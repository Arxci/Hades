#pragma once

#include "CoreMinimal.h"

#include "Kismet/KismetMathLibrary.h"
#include "Slate/WidgetTransform.h"

#include "ModularUITransition.generated.h"

class UCurveFloat;
class UWidget;


/** Which way a widget slides in as it opens, and back out as it closes. */
UENUM(BlueprintType)
enum class EModularUISlide : uint8
{
	/**
	 * Away from where it opened, as if from behind it: from the left for a menu opened on an item's right, and down from the cursor for
	 * one opened below it. Without an origin, such as for a widget laid out in the designer, it slides up.
	 */
	FromOrigin,
	FromAbove,
	FromBelow,
	FromLeft,
	FromRight
};


/** What a transition's Slide Distance is measured in. */
UENUM(BlueprintType)
enum class EModularUISlideUnit : uint8
{
	/** Pixels. */
	Pixels,
	/**
	 * A fraction of the widget's own size along the slide: its width for a slide from the side, its height for one from above or below. 1
	 * brings it in from just past where it ends, such as out from behind the edge of a panel that clips it.
	 */
	OwnSize
};


/** Where a widget grows from as it opens, and shrinks back to as it closes. */
UENUM(BlueprintType)
enum class EModularUIScaleOrigin : uint8
{
	/**
	 * Its corner nearest where it opened, such as its top left for a menu opened on an item's right, level with its top, so it grows out
	 * of the item. Without an origin, its middle.
	 */
	Origin,
	/** Its middle. */
	Center
};


/**
 * How a widget comes in as it opens, or goes as it closes: a fade, a slide and a growth, each optional, played together once Delay has
 * passed. Played in code, so it follows wherever the widget opens, such as out from an item, without a widget animation.
 */
USTRUCT(BlueprintType)
struct MODULARUI_API FModularUITransition
{
	GENERATED_BODY()

	/** Whether it changes anything. Without, the widget shows or hides at once. */
	bool HasEffect() const;

	/** How far along it looks at Time, from 0 to 1 of its Duration: by Curve, or else by Easing. */
	float Ease(float Time) const;

	/** How long it waits before it starts, such as to bring a title in after its panel. Coming in, the widget stays hidden meanwhile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "0", Units = "s"))
	float Delay = 0.0f;

	/** How long it takes once it starts. 0 shows or hides the widget at once, once Delay has passed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "0", Units = "s"))
	float Duration = 0.1f;

	/** How it speeds up and slows down: Linear keeps one pace, and Ease Out starts quick and settles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	TEnumAsByte<EEasingFunc::Type> Easing = EEasingFunc::EaseOut;

	/** How sharp Ease In, Ease Out and Ease In Out are: 2 is gentle, and higher is sharper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "1"))
	float EasingStrength = 2.0f;

	/**
	 * Used instead of Easing: across times 0 to 1, how far along it looks, from 0 to 1. Rise past 1 before settling on it to overshoot,
	 * such as a widget that slides or grows a little too far and springs back. Either way, it starts and ends exactly as it should.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	TObjectPtr<UCurveFloat> Curve;

	/** Fades in from Fade From. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	bool bFade = true;

	/** The opacity it fades from: 0 is unseen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bFade", EditConditionHides))
	float FadeFrom = 0.0f;

	/** Slides in from Slide Distance away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	bool bSlide = true;

	/** How far it slides: in pixels, or as a fraction of its own size, as Slide Unit says. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (ClampMin = "0", EditCondition = "bSlide", EditConditionHides))
	float SlideDistance = 16.0f;

	/** What Slide Distance is measured in: pixels, or the widget's own size along the slide, so 1 slides it in from just outside where it ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (EditCondition = "bSlide", EditConditionHides))
	EModularUISlideUnit SlideUnit = EModularUISlideUnit::Pixels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (EditCondition = "bSlide", EditConditionHides))
	EModularUISlide SlideFrom = EModularUISlide::FromOrigin;

	/** Grows from Scale From of its size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
	bool bScale = false;

	/**
	 * The size it grows from, as a fraction of its own: 0.9 for a slight pop, or 0 from nothing. Leave one axis at 1 to unfold it along the
	 * other, such as 1 and 0 to drop it open from its top.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (EditCondition = "bScale", EditConditionHides))
	FVector2D ScaleFrom = FVector2D(0.9, 0.9);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition", meta = (EditCondition = "bScale", EditConditionHides))
	EModularUIScaleOrigin ScaleOrigin = EModularUIScaleOrigin::Origin;
};


/**
 * Plays transitions on widgets: fades one, such as a menu's root, and slides and grows another, such as the panel the menu is drawn in.
 * Each transition carries on from however the widgets look as it starts, so a widget closed while it's still opening goes back smoothly.
 * The widgets only change as Advance moves it on, such as from a ticker, so it plays whether or not they tick, with the game paused too.
 * Transitions add to the render opacity and transform the widgets have when Init is called, such as those set in the designer.
 */
struct MODULARUI_API FModularUITransitionPlayer
{
	/** Fade Target fades, and Move Target slides and grows. Either can be null, or both the same widget. */
	void Init(UWidget* InFadeTarget, UWidget* InMoveTarget);

	/**
	 * Where transitions From Origin come from: Away, the way from the origin to the widget, such as right for a menu on an item's right;
	 * and Corner, the widget's corner nearest the origin, from 0, 0 at its top left to 1, 1 at its bottom right. Takes effect at once, even
	 * mid-way. Until it's set, widgets slide up and grow from their middle.
	 */
	void SetOrigin(const FVector2D& InAway, const FVector2D& InCorner);

	/** Comes in with Transition, from hidden as it says to shown, once its Delay has passed. Shown at once when it has no effect. */
	void PlayIn(const FModularUITransition& InTransition);

	/**
	 * Goes back the way the last Play In came, from wherever it is now, until hidden as it started: the opening played backwards. Plays
	 * nothing when the last Play In had no effect, nor while going out. One still waiting out its Delay is hidden already, and stops.
	 */
	void ReverseIn();

	/**
	 * Goes out with Transition, from however it looks now, until hidden as it says, once its Delay has passed. Quicker the less shown it
	 * still was, such as when it's still coming in. Hidden at once when it has no effect.
	 */
	void PlayOut(const FModularUITransition& InTransition);

	/** Stops where it is. */
	void Stop();

	/** Jumps to where the transition playing ends, at once: shown after Play In, hidden after Play Out or Reverse In. */
	void Finish();

	/** Whether a transition is still playing, so Advance has more to do. */
	bool IsPlaying() const { return Rate != 0.0f; }

	/** Moves the transition on by Delta Time, in seconds. True while it's still playing. */
	bool Advance(float DeltaTime);

private:
	/** How the widgets look partway through: the fade's opacity, the slide's offset and the growth's scale. */
	struct FLook
	{
		float Opacity = 1.0f;
		FVector2D Offset = FVector2D::ZeroVector;
		FVector2D Scale = FVector2D::UnitVector;
	};

	static FLook Lerp(const FLook& From, const FLook& To, float Alpha);

	/** How the widgets look hidden by Transition: before it comes in, or once it has gone out. What it doesn't change stays as in Look. */
	FLook GetHiddenLook(const FModularUITransition& InTransition, const FLook& Look) const;

	/** How far Transition slides along Direction, a unit offset: its Slide Distance in pixels, or times Move Target's size along Direction. */
	double GetSlideLength(const FModularUITransition& InTransition, const FVector2D& Direction) const;
	FLook GetLook() const;
	void Apply(const FLook& Look) const;

	TWeakObjectPtr<UWidget> FadeTarget;
	TWeakObjectPtr<UWidget> MoveTarget;

	/** How the widgets were at Init, which transitions add to. */
	float BaseOpacity = 1.0f;
	FWidgetTransform BaseTransform;
	FVector2D BasePivot = FVector2D(0.5, 0.5);

	FVector2D Away = FVector2D(0.0, -1.0);
	FVector2D Corner = FVector2D(0.5, 0.5);

	/** The transition playing, or played last. Whoever passed it in keeps its Curve. */
	FModularUITransition Transition;

	/** Transition goes out, from Out From to hidden, rather than in, from hidden to shown. */
	bool bOut = false;
	FLook OutFrom;

	/** Grows from Corner rather than the middle, as the transition that grew it says. */
	bool bGrowFromCorner = false;

	/**
	 * How far along Transition is, from 0 to 1, and how fast it moves, per second: below 0 to go back the way it came in. It only moves
	 * once Wait, what's left of its Delay, has run out.
	 */
	float Position = 1.0f;
	float Rate = 0.0f;
	float Wait = 0.0f;
};
