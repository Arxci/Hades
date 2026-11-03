#include "HadesWorldMarker.h"

#include "Components/SceneComponent.h"

#include "HadesWorldMarkerManagerComponent.h"
#include "HadesWorldMarkerWidget.h"

//Setup
void UHadesWorldMarker::SetAnchor(USceneComponent* InAnchor, FName InSocketName)
{
	Anchor = InAnchor;
	SocketName = InSocketName;
}

FVector UHadesWorldMarker::GetAnchorLocation() const
{
	return Anchor.IsValid() ? Anchor->GetSocketLocation(SocketName) : FVector::ZeroVector;
}

//State
void UHadesWorldMarker::SetStateTag(FGameplayTag Tag, bool bEnabled)
{
	if (StateTags.HasTagExact(Tag) == bEnabled)
	{
		return;
	}

	if (bEnabled)
	{
		StateTags.AddTag(Tag);
	}
	else
	{
		StateTags.RemoveTag(Tag);
	}

	if (UHadesWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnStateTagChanged(Tag, bEnabled);
	}
}

void UHadesWorldMarker::SetProgress(float NewProgress)
{
	if (FMath::IsNearlyEqual(Progress, NewProgress))
	{
		return;
	}

	Progress = NewProgress;

	if (UHadesWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnProgressChanged(Progress);
	}
}

void UHadesWorldMarker::RefreshWidget()
{
	if (UHadesWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnContextChanged();
	}
}

//Registration
void UHadesWorldMarker::Unregister()
{
	if (UHadesWorldMarkerManagerComponent* MarkerManager = Manager.Get())
	{
		MarkerManager->RemoveMarker(this);
	}
}
