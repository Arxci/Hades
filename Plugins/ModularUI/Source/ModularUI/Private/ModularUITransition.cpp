#include "ModularUITransition.h"

#include "Components/Widget.h"
#include "Curves/CurveFloat.h"

namespace
{
	/** Where a slide starts, as a unit offset from where it ends. Away is the way from the origin to the widget. */
	FVector2D GetSlideStart(EModularUISlide SlideFrom, const FVector2D& Away)
	{
		switch (SlideFrom)
		{
		case EModularUISlide::FromAbove:  return FVector2D(0.0, -1.0);
		case EModularUISlide::FromBelow:  return FVector2D(0.0, 1.0);
		case EModularUISlide::FromLeft:   return FVector2D(-1.0, 0.0);
		case EModularUISlide::FromRight:  return FVector2D(1.0, 0.0);
		case EModularUISlide::FromOrigin: break;
		}
		return -Away;
	}

	/** How fast to cover a transition that takes Seconds, as a fraction of it per second. One that takes no time ends on the next frame. */
	float GetRate(float Seconds)
	{
		return 1.0f / FMath::Max(Seconds, UE_KINDA_SMALL_NUMBER);
	}
}

// Transition
bool FModularUITransition::HasEffect() const
{
	return (Duration > 0.0f || Delay > 0.0f) && (bFade || bSlide || bScale);
}

float FModularUITransition::Ease(float Time) const
{
	const float Alpha = FMath::Clamp(Time, 0.0f, 1.0f);
	if (Curve)
	{
		return Curve->GetFloatValue(Alpha);
	}
	return static_cast<float>(UKismetMathLibrary::Ease(0.0f, 1.0f, Alpha, Easing, EasingStrength));
}

// Player
void FModularUITransitionPlayer::Init(UWidget* InFadeTarget, UWidget* InMoveTarget)
{
	FadeTarget = InFadeTarget;
	MoveTarget = InMoveTarget;
	BaseOpacity = InFadeTarget ? InFadeTarget->GetRenderOpacity() : 1.0f;
	BaseTransform = InMoveTarget ? InMoveTarget->GetRenderTransform() : FWidgetTransform();
	BasePivot = InMoveTarget ? InMoveTarget->GetRenderTransformPivot() : FVector2D(0.5, 0.5);

	Transition = FModularUITransition();
	bOut = false;
	Position = 1.0f;
	Rate = 0.0f;
	Wait = 0.0f;
}

void FModularUITransitionPlayer::SetOrigin(const FVector2D& InAway, const FVector2D& InCorner)
{
	Away = InAway;
	Corner = InCorner;
	if (IsPlaying())
	{
		Apply(GetLook());
	}
}

void FModularUITransitionPlayer::PlayIn(const FModularUITransition& InTransition)
{
	Transition = InTransition;
	bOut = false;
	bGrowFromCorner = InTransition.bScale && InTransition.ScaleOrigin == EModularUIScaleOrigin::Origin;

	const bool bPlays = InTransition.HasEffect();
	Position = bPlays ? 0.0f : 1.0f;
	Rate = bPlays ? GetRate(InTransition.Duration) : 0.0f;
	Wait = bPlays ? InTransition.Delay : 0.0f;
	Apply(GetLook());
}

void FModularUITransitionPlayer::ReverseIn()
{
	if (bOut || !Transition.HasEffect())
	{
		return;
	}

	// One still waiting out its delay hasn't moved, so it's hidden already.
	Wait = 0.0f;
	Rate = Position > 0.0f ? -GetRate(Transition.Duration) : 0.0f;
}

void FModularUITransitionPlayer::PlayOut(const FModularUITransition& InTransition)
{
	if (bOut && IsPlaying())
	{
		return;
	}

	// From however it looks now, and as much quicker as it's less shown, so a widget closed as it opens doesn't linger.
	const float Shown = bOut ? 0.0f : Position;
	OutFrom = GetLook();
	Transition = InTransition;
	bOut = true;
	if (InTransition.bScale)
	{
		bGrowFromCorner = InTransition.ScaleOrigin == EModularUIScaleOrigin::Origin;
	}

	const bool bPlays = InTransition.HasEffect() && Shown > 0.0f;
	Position = bPlays ? 0.0f : 1.0f;
	Rate = bPlays ? GetRate(InTransition.Duration * Shown) : 0.0f;
	Wait = bPlays ? InTransition.Delay : 0.0f;
	Apply(GetLook());
}

void FModularUITransitionPlayer::Stop()
{
	Rate = 0.0f;
	Wait = 0.0f;
}

void FModularUITransitionPlayer::Finish()
{
	if (!IsPlaying())
	{
		return;
	}

	// Coming in ends shown, at 1. Going out ends hidden, at 1 too; going back the way it came in ends hidden, at 0.
	Position = Rate > 0.0f ? 1.0f : 0.0f;
	Rate = 0.0f;
	Wait = 0.0f;
	Apply(GetLook());
}

bool FModularUITransitionPlayer::Advance(float DeltaTime)
{
	if (!IsPlaying())
	{
		return false;
	}

	// The delay comes off first, and only what's left of the frame moves it on.
	if (Wait > 0.0f)
	{
		const float Waited = FMath::Min(Wait, DeltaTime);
		Wait -= Waited;
		DeltaTime -= Waited;
		if (Wait > 0.0f)
		{
			return true;
		}
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

FModularUITransitionPlayer::FLook FModularUITransitionPlayer::Lerp(const FLook& From, const FLook& To, float Alpha)
{
	FLook Look;
	Look.Opacity = From.Opacity + (To.Opacity - From.Opacity) * Alpha;
	Look.Offset = From.Offset + (To.Offset - From.Offset) * Alpha;
	Look.Scale = From.Scale + (To.Scale - From.Scale) * Alpha;
	return Look;
}

FModularUITransitionPlayer::FLook FModularUITransitionPlayer::GetHiddenLook(const FModularUITransition& InTransition, const FLook& Look) const
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

FModularUITransitionPlayer::FLook FModularUITransitionPlayer::GetLook() const
{
	// Exactly as it starts and ends, whatever a curve does on the way. Going out, what the transition doesn't change stays where it was.
	const float Alpha = Position <= 0.0f ? 0.0f : Position >= 1.0f ? 1.0f : Transition.Ease(Position);
	const FLook Shown = FLook();
	return bOut ? Lerp(OutFrom, GetHiddenLook(Transition, OutFrom), Alpha) : Lerp(GetHiddenLook(Transition, Shown), Shown, Alpha);
}

void FModularUITransitionPlayer::Apply(const FLook& Look) const
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

		// Grown from the corner nearest the origin, or its middle, only while grown, so the designer's own pivot holds the rest of the time.
		const bool bGrown = !Look.Scale.Equals(FVector2D::UnitVector);
		Moving->SetRenderTransformPivot(!bGrown ? BasePivot : bGrowFromCorner ? Corner : FVector2D(0.5, 0.5));
	}
}
