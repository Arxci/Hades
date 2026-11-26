#include "WorldMarkers/ModularWorldMarkerManagerComponent.h"

#include "GameFramework/Controller.h"

#include "WorldMarkers/ModularWorldMarker.h"

UModularWorldMarkerManagerComponent* UModularWorldMarkerManagerComponent::Get(const AController* Controller)
{
	return Controller ? Controller->FindComponentByClass<UModularWorldMarkerManagerComponent>() : nullptr;
}

void UModularWorldMarkerManagerComponent::AddMarker(UModularWorldMarker* Marker)
{
	if (!Marker || Markers.Contains(Marker))
	{
		return;
	}

	Marker->Manager = this;
	Markers.Add(Marker);
	OnMarkerAdded.Broadcast(Marker);
}

void UModularWorldMarkerManagerComponent::RemoveMarker(UModularWorldMarker* Marker)
{
	if (Markers.Remove(Marker) == 0)
	{
		return;
	}

	Marker->Manager = nullptr;
	OnMarkerRemoved.Broadcast(Marker);
}
