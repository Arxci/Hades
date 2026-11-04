#pragma once

#include "CoreMinimal.h"

#include "AsyncMixin.h"
#include "Blueprint/UserWidgetPool.h"
#include "Components/Widget.h"
#include "Widgets/Layout/SConstraintCanvas.h"

#include "HadesWorldMarkerLayer.generated.h"

class SHadesWorldMarkerCanvas;
class UHadesWorldMarker;
class UHadesWorldMarkerManagerComponent;
class UHadesWorldMarkerWidget;

USTRUCT()
struct FHadesWorldMarkerLayerEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UHadesWorldMarker> Marker;

	UPROPERTY()
	TObjectPtr<UHadesWorldMarkerWidget> Widget;

	SConstraintCanvas::FSlot* Slot = nullptr;

	/** Removed from the manager. Fades out, then its widget returns to the pool. */
	bool bRemoving = false;
};

/** Draws the owning player's world markers. Place it in the HUD so it fills the screen. */
UCLASS()
class HADES_API UHadesWorldMarkerLayer : public UWidget, public FAsyncMixin
{
	GENERATED_BODY()

public:
	UHadesWorldMarkerLayer(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget Interface

private:
	EActiveTimerReturnType UpdateMarkers(double InCurrentTime, float InDeltaTime);

	void BindToManager();
	void HandleMarkerAdded(UHadesWorldMarker* Marker);
	void HandleMarkerRemoved(UHadesWorldMarker* Marker);

	FHadesWorldMarkerLayerEntry* FindEntry(const UHadesWorldMarker* Marker);
	void AddEntry(UHadesWorldMarker* Marker, TSubclassOf<UHadesWorldMarkerWidget> WidgetClass);
	void RemoveEntry(int32 Index);

	UPROPERTY(Transient)
	FUserWidgetPool WidgetPool;

	UPROPERTY(Transient)
	TArray<FHadesWorldMarkerLayerEntry> Entries;

	TWeakObjectPtr<UHadesWorldMarkerManagerComponent> Manager;

	TSharedPtr<SHadesWorldMarkerCanvas> Canvas;
};
