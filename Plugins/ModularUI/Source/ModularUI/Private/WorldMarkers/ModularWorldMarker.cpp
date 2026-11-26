#include "WorldMarkers/ModularWorldMarker.h"

#include "Components/SceneComponent.h"

#include "WorldMarkers/ModularWorldMarkerManagerComponent.h"
#include "WorldMarkers/ModularWorldMarkerWidget.h"

// Setup
void UModularWorldMarker::SetAnchor(USceneComponent* InAnchor, FName InSocketName)
{
	Anchor = InAnchor;
	SocketName = InSocketName;
}

FVector UModularWorldMarker::GetAnchorLocation() const
{
	return Anchor.IsValid() ? Anchor->GetSocketLocation(SocketName) + WorldOffset : WorldOffset;
}

// State
void UModularWorldMarker::SetStateTag(FGameplayTag Tag, bool bEnabled)
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

	if (UModularWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnStateTagChanged(Tag, bEnabled);
	}
}

void UModularWorldMarker::SetProgress(float NewProgress)
{
	if (FMath::IsNearlyEqual(Progress, NewProgress))
	{
		return;
	}

	Progress = NewProgress;

	if (UModularWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnProgressChanged(Progress);
	}
}

// Events
void UModularWorldMarker::NotifyEvent(FGameplayTag EventTag)
{
	if (UModularWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnEventReceived(EventTag);
	}
}

void UModularWorldMarker::RefreshWidget()
{
	if (UModularWorldMarkerWidget* MarkerWidget = Widget.Get())
	{
		MarkerWidget->NativeOnContextChanged();
	}
}

// Registration
void UModularWorldMarker::Unregister()
{
	if (UModularWorldMarkerManagerComponent* MarkerManager = Manager.Get())
	{
		MarkerManager->RemoveMarker(this);
	}
}
