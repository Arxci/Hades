#include "InventoryMenuTransition.h"

#include "Components/Widget.h"
#include "Curves/CurveFloat.h"

namespace
{
	/** Where a slide starts, as a unit offset from where it ends. Away is the way from the item to the menu. */
	FVector2D GetSlideStart(EInventoryMenuSlide SlideFrom, const FVector2D& Away)
	{
		switch (SlideFrom)
		{
		case EInventoryMenuSlide::FromAbove: return FVector2D(0.0, -1.0);
		case EInventoryMenuSlide::FromBelow: return FVector2D(0.0, 1.0);
		case EInventoryMenuSlide::FromLeft:  return FVector2D(-1.0, 0.0);
		case EInventoryMenuSlide::FromRight: return FVector2D(1.0, 0.0);
		case EInventoryMenuSlide::FromItem:  break;
		}
		return -Away;
	}
}

// Transition
bool FInventoryMenuTransition::HasEffect() const
{
	return Duration > 0.0f && (bFade || bSlide || bScale);
}

float FInventoryMenuTransition::Ease(float Time) const
{
	const float Alpha = FMath::Clamp(Time, 0.0f, 1.0f);
	if (Curve)
	{
		return Curve->GetFloatValue(Alpha);
	}
	return static_cast<float>(UKismetMathLibrary::Ease(0.0f, 1.0f, Alpha, Easing, EasingStrength));
}

// Player
void FInventoryMenuTransitionPlayer::Init(UWidget* InFadeTarget, UWidget* InMoveTarget)
{
	FadeTarget = InFadeTarget;
	MoveTarget = InMoveTarget;
	BaseOpacity = InFadeTarget ? InFadeTarget->GetRenderOpacity() : 1.0f;
	BaseTransform = InMoveTarget ? InMoveTarget->GetRenderTransform() : FWidgetTransform();
	BasePivot = InMoveTarget ? InMoveTarget->GetRenderTransformPivot() : FVector2D(0.5, 0.5);

	Transition = FInventoryMenuTransition();
	bOut = false;
	Position = 1.0f;
	Rate = 0.0f;
}

void FInventoryMenuTransitionPlayer::SetOrigin(const FVector2D& InAway, const FVector2D& InCorner)
{
	Away = InAway;
	Corner = InCorner;
	if (IsPlaying())
	{
		Apply(GetLook());
	}
}

void FInventoryMenuTransitionPlayer::PlayIn(const FInventoryMenuTransition& InTransition)
{
	Transition = InTransition;
	bOut = false;
	bGrowFromCorner = InTransition.bScale && InTransition.ScaleOrigin == EInventoryMenuScaleOrigin::Item;

	const bool bPlays = InTransition.HasEffect();
	Position = bPlays ? 0.0f : 1.0f;
	Rate = bPlays ? 1.0f / InTransition.Duration : 0.0f;
	Apply(GetLook());
}

void FInventoryMenuTransitionPlayer::ReverseIn()
{
	if (!bOut && Transition.HasEffect() && Position > 0.0f)
	{
		Rate = -1.0f / Transition.Duration;
	}
}

void FInventoryMenuTransitionPlayer::PlayOut(const FInventoryMenuTransition& InTransition)
{
	if (bOut && IsPlaying())
	{
		return;
	}

	// From however it looks now, and as much quicker as it's less shown, so a menu closed as it opens doesn't linger.
	const float Shown = bOut ? 0.0f : Position;
	OutFrom = GetLook();
	Transition = InTransition;
	bOut = true;
	if (InTransition.bScale)
	{
		bGrowFromCorner = InTransition.ScaleOrigin == EInventoryMenuScaleOrigin::Item;
	}

	const float Length = InTransition.Duration * Shown;
	const bool bPlays = InTransition.HasEffect() && Length > 0.0f;
	Position = bPlays ? 0.0f : 1.0f;
	Rate = bPlays ? 1.0f / Length : 0.0f;
	Apply(GetLook());
}

bool FInventoryMenuTransitionPlayer::Advance(float DeltaTime)
{
	if (!IsPlaying())
	{
		return false;
	}

	Position = FMath::Clamp(Position + Rate * DeltaTime, 0.0f, 1.0f);
	const bool bEnded = Rate > 0.0f ? Position >= 1.0f : Position <= 0.0f;
	if (bEnded)
	{
		Rate = 0.0f;
	}
	Apply(GetLook());
	return !bEnded;
}

FInventoryMenuTransitionPlayer::FLook FInventoryMenuTransitionPlayer::Lerp(const FLook& From, const FLook& To, float Alpha)
{
	FLook Look;
	Look.Opacity = From.Opacity + (To.Opacity - From.Opacity) * Alpha;
	Look.Offset = From.Offset + (To.Offset - From.Offset) * Alpha;
	Look.Scale = From.Scale + (To.Scale - From.Scale) * Alpha;
	return Look;
}

FInventoryMenuTransitionPlayer::FLook FInventoryMenuTransitionPlayer::GetHiddenLook(const FInventoryMenuTransition& InTransition, const FLook& Look) const
{
	FLook Hidden = Look;
	if (InTransition.bFade)
	{
		Hidden.Opacity = InTransition.FadeFrom;
	}
	if (InTransition.bSlide)
	{
		Hidden.Offset = GetSlideStart(InTransition.SlideFrom, Away) * InTransition.SlideDistance;
	}
	if (InTransition.bScale)
	{
		Hidden.Scale = InTransition.ScaleFrom;
	}
	return Hidden;
}

FInventoryMenuTransitionPlayer::FLook FInventoryMenuTransitionPlayer::GetLook() const
{
	// Exactly as it starts and ends, whatever a curve does on the way. Going out, what the transition doesn't change stays where it was.
	const float Alpha = Position <= 0.0f ? 0.0f : Position >= 1.0f ? 1.0f : Transition.Ease(Position);
	const FLook Shown = FLook();
	return bOut ? Lerp(OutFrom, GetHiddenLook(Transition, OutFrom), Alpha) : Lerp(GetHiddenLook(Transition, Shown), Shown, Alpha);
}

void FInventoryMenuTransitionPlayer::Apply(const FLook& Look) const
{
	if (UWidget* Fading = FadeTarget.Get())
	{
		// An overshooting curve can carry the fade past either end.
		Fading->SetRenderOpacity(BaseOpacity * FMath::Clamp(Look.Opacity, 0.0f, 1.0f));
	}

	if (UWidget* Moving = MoveTarget.Get())
	{
		FWidgetTransform Transform = BaseTransform;
		Transform.Translation += Look.Offset;
		Transform.Scale *= Look.Scale;
		Moving->SetRenderTransform(Transform);

		// Grown from the corner nearest the item, or its middle, only while grown, so the designer's own pivot holds the rest of the time.
		const bool bGrown = !Look.Scale.Equals(FVector2D::UnitVector);
		Moving->SetRenderTransformPivot(!bGrown ? BasePivot : bGrowFromCorner ? Corner : FVector2D(0.5, 0.5));
	}
}
