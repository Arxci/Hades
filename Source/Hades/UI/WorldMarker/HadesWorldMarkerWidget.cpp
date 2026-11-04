#include "HadesWorldMarkerWidget.h"

#include "Components/SceneComponent.h"
#include "Components/Widget.h"

#include "Hades/UI/WorldMarker/HadesWorldMarker.h"

void UHadesWorldMarkerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EdgeArrow)
	{
		EdgeArrow->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//Context
void UHadesWorldMarkerWidget::SetMarker(UHadesWorldMarker* InMarker)
{
	Marker = InMarker;

	VisibilityAlpha = 0.0f;
	EdgeAlpha = 1.0f;
	ApplyOpacity();
	SetClampedToEdge(false, 0.0f);

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
	if (EdgeArrow && bClamped)
	{
		EdgeArrow->SetRenderTransformAngle(Angle);
	}

	if (bClampedToEdge == bClamped)
	{
		return;
	}

	bClampedToEdge = bClamped;

	if (EdgeArrow)
	{
		EdgeArrow->SetVisibility(bClamped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	OnEdgeClampChanged(bClamped);
}

//Visibility
bool UHadesWorldMarkerWidget::UpdateVisibilityFade(bool bVisible, float DeltaTime)
{
	const float Target = bVisible ? 1.0f : 0.0f;
	const float FadeTime = bVisible ? FadeInTime : FadeOutTime;
	const float NewVisibilityAlpha = FadeTime > 0.0f ? FMath::FInterpConstantTo(VisibilityAlpha, Target, DeltaTime, 1.0f / FadeTime) : Target;

	if (VisibilityAlpha != NewVisibilityAlpha)
	{
		VisibilityAlpha = NewVisibilityAlpha;
		ApplyOpacity();
	}

	return VisibilityAlpha > 0.0f;
}

void UHadesWorldMarkerWidget::ApplyOpacity()
{
	SetRenderOpacity(FMath::SmoothStep(0.0f, 1.0f, VisibilityAlpha) * EdgeAlpha);
}
