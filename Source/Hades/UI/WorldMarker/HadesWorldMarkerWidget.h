#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "HadesWorldMarkerWidget.generated.h"

class AActor;
class USceneComponent;
class UWidget;
class UHadesWorldMarker;

UENUM(BlueprintType)
enum class EHadesWorldMarkerEdgeMode : uint8
{
	/** Slides off the screen with its anchor. */
	None,
	/** Fades out as its anchor nears the screen edge. */
	Fade,
	/** Pins to the screen edge while its anchor is off screen, including behind the camera. */
	Clamp,
};

/**
 * Drawn over a UHadesWorldMarker by the HUD's UHadesWorldMarkerLayer. The marker's owner drives it with state tags,
 * progress and one-shot event tags; children react in the Blueprint events below.
 */
UCLASS(Abstract)
class HADES_API UHadesWorldMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Context
	/** Called by the layer when the widget is taken from its pool. Starts hidden, so it fades in. */
	void SetMarker(UHadesWorldMarker* InMarker);

	UFUNCTION(BlueprintPure, Category = "World Marker|Context")
	UHadesWorldMarker* GetMarker() const { return Marker.Get(); }

	UFUNCTION(BlueprintPure, Category = "World Marker|Context")
	USceneComponent* GetAnchor() const;

	UFUNCTION(BlueprintPure, Category = "World Marker|Context")
	UObject* GetContextObject() const;

	/** The anchor's owner. */
	UFUNCTION(BlueprintPure, Category = "World Marker|Context")
	AActor* GetContextActor() const;

	// State
	UFUNCTION(BlueprintPure, Category = "World Marker|State")
	bool HasStateTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "World Marker|State")
	float GetProgress() const;

	// Screen
	EHadesWorldMarkerEdgeMode GetEdgeMode() const { return EdgeMode; }
	float GetEdgePadding() const { return EdgePadding; }

	/** NormalizedPosition runs from -1 to 1 between opposite screen edges. */
	void UpdateEdgeFade(const FVector2D& NormalizedPosition);

	/** Angle is in degrees. 0 points right and 90 points down. */
	void SetClampedToEdge(bool bClamped, float Angle);

	// Visibility
	/** Returns true while any of the marker is showing. */
	bool UpdateVisibilityFade(bool bVisible, float DeltaTime);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** Also called when the widget is reused for another marker, so derive everything from the current marker. */
	virtual void NativeOnContextChanged();
	virtual void NativeOnStateTagChanged(const FGameplayTag& Tag, bool bAdded);
	virtual void NativeOnProgressChanged(float NewProgress);
	virtual void NativeOnEventReceived(const FGameplayTag& EventTag);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Marker|Context")
	void OnContextChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "World Marker|State")
	void OnStateTagChanged(FGameplayTag Tag, bool bAdded);

	/** 0 to 1. */
	UFUNCTION(BlueprintImplementableEvent, Category = "World Marker|State")
	void OnProgressChanged(float NewProgress);

	/** A one-shot moment, such as an interaction completing. Arrives before the state changes it causes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "World Marker|Events")
	void OnEventReceived(FGameplayTag EventTag);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Marker|Screen")
	void OnEdgeClampChanged(bool bClamped);

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen")
	EHadesWorldMarkerEdgeMode EdgeMode = EHadesWorldMarkerEdgeMode::None;

	/** Distance from the screen center. 1 is the middle of each screen edge; the corners are past 1. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen", meta = (EditCondition = "EdgeMode == EHadesWorldMarkerEdgeMode::Fade", EditConditionHides, ClampMin = "0"))
	float EdgeFadeStart = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen", meta = (EditCondition = "EdgeMode == EHadesWorldMarkerEdgeMode::Fade", EditConditionHides, ClampMin = "0"))
	float EdgeFadeEnd = 0.95f;

	/** Gap between a clamped marker and the screen edge. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen", meta = (EditCondition = "EdgeMode == EHadesWorldMarkerEdgeMode::Clamp", EditConditionHides, ClampMin = "0"))
	float EdgePadding = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Visibility", meta = (ClampMin = "0", Units = "s"))
	float FadeInTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Visibility", meta = (ClampMin = "0", Units = "s"))
	float FadeOutTime = 0.2f;

	/** Shown while clamped, rotated around its own center to point at the anchor. Draw it pointing right. */
	UPROPERTY(BlueprintReadOnly, Category = "World Marker|Screen", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EdgeArrow;

private:
	friend class UHadesWorldMarker;

	void ApplyOpacity();

	TWeakObjectPtr<UHadesWorldMarker> Marker;

	float VisibilityAlpha = 0.0f;
	float EdgeAlpha = 1.0f;
	bool bClampedToEdge = false;
};
