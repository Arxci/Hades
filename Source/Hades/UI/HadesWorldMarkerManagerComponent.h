#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "HadesWorldMarkerManagerComponent.generated.h"

class AController;
class UHadesWorldMarker;

/** Holds the world markers a player can see. Lives on the player controller, and the HUD's UHadesWorldMarkerLayer draws them. */
UCLASS(ClassGroup = (UI))
class HADES_API UHadesWorldMarkerManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "World Marker", meta = (DisplayName = "Get World Marker Manager"))
	static UHadesWorldMarkerManagerComponent* Get(const AController* Controller);

	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void AddMarker(UHadesWorldMarker* Marker);

	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void RemoveMarker(UHadesWorldMarker* Marker);

	const TArray<TObjectPtr<UHadesWorldMarker>>& GetMarkers() const { return Markers; }

	DECLARE_EVENT_OneParam(UHadesWorldMarkerManagerComponent, FHadesWorldMarkerEvent, UHadesWorldMarker*);
	FHadesWorldMarkerEvent OnMarkerAdded;
	FHadesWorldMarkerEvent OnMarkerRemoved;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHadesWorldMarker>> Markers;
};
