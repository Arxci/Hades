#include "HadesWorldMarkerManagerComponent.h"

#include "GameFramework/Controller.h"

#include "HadesWorldMarker.h"

UHadesWorldMarkerManagerComponent* UHadesWorldMarkerManagerComponent::Get(const AController* Controller)
{
	return Controller ? Controller->FindComponentByClass<UHadesWorldMarkerManagerComponent>() : nullptr;
}

void UHadesWorldMarkerManagerComponent::AddMarker(UHadesWorldMarker* Marker)
{
	if (!Marker || Markers.Contains(Marker))
	{
		return;
	}

	Marker->Manager = this;
	Markers.Add(Marker);
	OnMarkerAdded.Broadcast(Marker);
}

void UHadesWorldMarkerManagerComponent::RemoveMarker(UHadesWorldMarker* Marker)
{
	if (Markers.Remove(Marker) == 0)
	{
		return;
	}

	Marker->Manager = nullptr;
	OnMarkerRemoved.Broadcast(Marker);
}
