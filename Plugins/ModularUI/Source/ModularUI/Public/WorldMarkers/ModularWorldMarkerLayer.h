#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidgetPool.h"
#include "Components/Widget.h"
#include "Engine/StreamableManager.h"
#include "Widgets/Layout/SConstraintCanvas.h"

#include "ModularWorldMarkerLayer.generated.h"

class SModularWorldMarkerCanvas;
class UModularWorldMarker;
class UModularWorldMarkerManagerComponent;
class UModularWorldMarkerWidget;


USTRUCT()
struct FModularWorldMarkerLayerEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UModularWorldMarker> Marker;

	UPROPERTY()
	TObjectPtr<UModularWorldMarkerWidget> Widget;

	SConstraintCanvas::FSlot* Slot = nullptr;

	/** Removed from the manager. Fades out, then its widget returns to the pool. */
	bool bRemoving = false;
};


/**
 * Draws the owning player's world markers, each over its anchor, from the Modular World Marker Manager Component on their controller.
 * Place it in the HUD so it fills the screen. Marker widget classes load in the background the first time they're needed.
 */
UCLASS()
class MODULARUI_API UModularWorldMarkerLayer : public UWidget
{
	GENERATED_BODY()

public:
	UModularWorldMarkerLayer(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget Interface

private:
	EActiveTimerReturnType UpdateMarkers(double InCurrentTime, float InDeltaTime);

	void BindToManager();
	void HandleMarkerAdded(UModularWorldMarker* Marker);
	void HandleMarkerRemoved(UModularWorldMarker* Marker);

	FModularWorldMarkerLayerEntry* FindEntry(const UModularWorldMarker* Marker);
	void AddEntry(UModularWorldMarker* Marker, TSubclassOf<UModularWorldMarkerWidget> WidgetClass);
	void RemoveEntry(int32 Index);

	UPROPERTY(Transient)
	FUserWidgetPool WidgetPool;

	UPROPERTY(Transient)
	TArray<FModularWorldMarkerLayerEntry> Entries;

	TWeakObjectPtr<UModularWorldMarkerManagerComponent> Manager;

	TSharedPtr<SModularWorldMarkerCanvas> Canvas;

	/** Marker widget classes loading. */
	TArray<TSharedPtr<FStreamableHandle>> LoadHandles;
};
