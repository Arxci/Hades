#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "ModularWorldMarkerManagerComponent.generated.h"

class AController;
class UModularWorldMarker;


/** Holds the world markers a player can see. Add it to the player controller, and the HUD's Modular World Marker Layer draws them. */
UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class MODULARUI_API UModularWorldMarkerManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "World Marker", meta = (DisplayName = "Get World Marker Manager"))
	static UModularWorldMarkerManagerComponent* Get(const AController* Controller);

	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void AddMarker(UModularWorldMarker* Marker);

	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void RemoveMarker(UModularWorldMarker* Marker);

	const TArray<TObjectPtr<UModularWorldMarker>>& GetMarkers() const { return Markers; }

	DECLARE_EVENT_OneParam(UModularWorldMarkerManagerComponent, FModularWorldMarkerEvent, UModularWorldMarker*);
	FModularWorldMarkerEvent OnMarkerAdded;
	FModularWorldMarkerEvent OnMarkerRemoved;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UModularWorldMarker>> Markers;
};
