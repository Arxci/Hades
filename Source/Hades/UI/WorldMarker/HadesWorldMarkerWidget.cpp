#include "HadesWorldMarkerWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/SceneComponent.h"
#include "Components/Widget.h"

#include "Hades/UI/WorldMarker/HadesWorldMarker.h"

//Context
void UHadesWorldMarkerWidget::SetMarker(UHadesWorldMarker* InMarker)
{
	Marker = InMarker;

	StopAnimation(ShowAnimation);
	VisibilityAlpha = 0.0f;
	bShouldShow = false;
	bForceClamp = false;
	bReleasingClamp = false;
	ResetEdgeState();

	NativeOnContextChanged();
}

USceneComponent* UHadesWorldMarkerWidget::GetAnchor() const
{
	return Marker.IsValid() ? Marker->GetAnchor() : nullptr;
}

UObject* UHadesWorldMarkerWidget::GetContextObject() const
{
	return Marker.IsValid() ? Marker->GetContextObject() : nullptr;
}

AActor* UHadesWorldMarkerWidget::GetContextActor() const
{
	const USceneComponent* Anchor = GetAnchor();
	return Anchor ? Anchor->GetOwner() : nullptr;
}

void UHadesWorldMarkerWidget::NativeOnContextChanged()
{
	OnContextChanged();
}

//State
bool UHadesWorldMarkerWidget::HasStateTag(FGameplayTag Tag) const
{
	return Marker.IsValid() && Marker->HasStateTag(Tag);
}

float UHadesWorldMarkerWidget::GetProgress() const
{
	return Marker.IsValid() ? Marker->GetProgress() : 0.0f;
}

void UHadesWorldMarkerWidget::NativeOnStateTagChanged(const FGameplayTag& Tag, bool bAdded)
{
	OnStateTagChanged(Tag, bAdded);
}

void UHadesWorldMarkerWidget::NativeOnProgressChanged(float NewProgress)
{
	OnProgressChanged(NewProgress);
}

//Events
void UHadesWorldMarkerWidget::NativeOnEventReceived(const FGameplayTag& EventTag)
{
	OnEventReceived(EventTag);
}

//Screen
void UHadesWorldMarkerWidget::UpdateEdgeFade(const FVector2D& NormalizedPosition)
{
	const float NewEdgeAlpha = 1.0f - FMath::SmoothStep(EdgeFadeStart, EdgeFadeEnd, static_cast<float>(NormalizedPosition.Size()));
	if (EdgeAlpha != NewEdgeAlpha)
	{
		EdgeAlpha = NewEdgeAlpha;
		ApplyOpacity();
	}
}

void UHadesWorldMarkerWidget::SetClampedToEdge(bool bClamped, float Angle)
{
	if (EdgeArrow)
	{
		// The arrow is drawn pointing down, which is 90 degrees.
		EdgeArrow->SetRenderTransformAngle(bClamped ? Angle - 90.0f : 0.0f);
		EdgeArrow->SetVisibility(bClamped || bShowEdgeArrowOnScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bClampedToEdge != bClamped)
	{
		bClampedToEdge = bClamped;
		OnEdgeClampChanged(bClamped);
	}
}

void UHadesWorldMarkerWidget::SetForceClamp(bool bInForceClamp)
{
	const EHadesWorldMarkerEdgeMode OldEdgeMode = GetEdgeMode();
	const bool bCancelsRelease = bReleasingClamp && bInForceClamp;
	bForceClamp = bInForceClamp;
	// Released while clamped, the marker fades out at the edge instead of jumping to its off-screen anchor.
	bReleasingClamp = !bInForceClamp && bClampedToEdge;

	if (GetEdgeMode() != OldEdgeMode)
	{
		ResetEdgeState();
	}
	else if (bCancelsRelease)
	{
		EdgeAlpha = 1.0f;
		ApplyOpacity();
	}
}

void UHadesWorldMarkerWidget::ResetEdgeState()
{
	EdgeAlpha = 1.0f;
	ApplyOpacity();
	SetClampedToEdge(false, 0.0f);
}

void UHadesWorldMarkerWidget::UpdateClampRelease(float DeltaTime)
{
	EdgeAlpha = FadeOutTime > 0.0f ? FMath::FInterpConstantTo(EdgeAlpha, 0.0f, DeltaTime, 1.0f / FadeOutTime) : 0.0f;
	ApplyOpacity();

	if (!bClampedToEdge || (EdgeAlpha == 0.0f && !IsTransitionPlaying()))
	{
		bReleasingClamp = false;
		ResetEdgeState();
	}
}

//Visibility
bool UHadesWorldMarkerWidget::UpdateShowHide(bool bShow, float DeltaTime)
{
	if (bReleasingClamp)
	{
		UpdateClampRelease(DeltaTime);
	}

	bool bShowing;
	float NewVisibilityAlpha;
	if (ShowAnimation)
	{
		// An animation started here can apply its first frame on the next tick, so stay transparent until it has.
		const bool bStartingFromHidden = bShow && !bShouldShow && VisibilityAlpha == 0.0f;
		if (bShow != bShouldShow)
		{
			bShouldShow = bShow;
			if (bShow)
			{
				PlayAnimationForward(ShowAnimation);
			}
			else
			{
				PlayAnimationReverse(ShowAnimation);
			}
		}
		bShowing = bShow || IsAnimationPlaying(ShowAnimation);
		NewVisibilityAlpha = bShowing && !bStartingFromHidden ? 1.0f : 0.0f;
	}
	else
	{
		const float Target = bShow ? 1.0f : 0.0f;
		const float FadeTime = bShow ? FadeInTime : FadeOutTime;
		NewVisibilityAlpha = FadeTime > 0.0f ? FMath::FInterpConstantTo(VisibilityAlpha, Target, DeltaTime, 1.0f / FadeTime) : Target;
		bShowing = NewVisibilityAlpha > 0.0f;
	}

	if (VisibilityAlpha != NewVisibilityAlpha)
	{
		VisibilityAlpha = NewVisibilityAlpha;
		ApplyOpacity();
	}

	return bShowing;
}

void UHadesWorldMarkerWidget::ApplyOpacity()
{
	SetRenderOpacity(FMath::SmoothStep(0.0f, 1.0f, VisibilityAlpha) * EdgeAlpha);
}
