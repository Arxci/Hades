#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "HadesWorldMarkerWidget.generated.h"

class AActor;
class USceneComponent;
class UWidget;
class UWidgetAnimation;
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
	/** Called by the layer when the widget is taken from its pool. Starts hidden, so it fades or animates in. */
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
	float GetMarkerScale() const { return MarkerScale; }
	FVector2D GetAnchorAlignment() const { return AnchorAlignment; }
	EHadesWorldMarkerEdgeMode GetEdgeMode() const { return bForceClamp || bReleasingClamp ? EHadesWorldMarkerEdgeMode::Clamp : EdgeMode; }
	const FMargin& GetEdgePadding() const { return EdgePadding; }

	/** NormalizedPosition runs from -1 to 1 between opposite screen edges. */
	void UpdateEdgeFade(const FVector2D& NormalizedPosition);

	/** Angle is in degrees. 0 points right and 90 points down. */
	void SetClampedToEdge(bool bClamped, float Angle);

	// Visibility
	/** Fades or animates toward shown or hidden. Returns true while the marker should stay on screen. */
	bool UpdateShowHide(bool bShow, float DeltaTime);

protected:
	/** Also called when the widget is reused for another marker, so derive everything from the current marker. */
	virtual void NativeOnContextChanged();
	virtual void NativeOnStateTagChanged(const FGameplayTag& Tag, bool bAdded);
	virtual void NativeOnProgressChanged(float NewProgress);
	virtual void NativeOnEventReceived(const FGameplayTag& EventTag);

	/** While set, the marker clamps to the screen edge whatever its Edge Mode. */
	void SetForceClamp(bool bInForceClamp);

	/** True while a transition that should finish on screen is playing. A marker leaving a forced clamp waits for it before moving off screen, since UMG animations don't advance on widgets that aren't drawn. */
	virtual bool IsTransitionPlaying() const { return false; }

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

	/** Scales the whole marker while keeping text and images sharp. Edge Padding scales with it, so set that as if this were 1. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen", meta = (ClampMin = "0.1"))
	float MarkerScale = 1.0f;

	/** The point of the widget placed on the anchor, from (0, 0) at the top left to (1, 1) at the bottom right. (0.5, 1) suits markers that point down at their anchor. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen")
	FVector2D AnchorAlignment = FVector2D(0.5, 0.5);

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen")
	EHadesWorldMarkerEdgeMode EdgeMode = EHadesWorldMarkerEdgeMode::None;

	/** Distance from the screen center. 1 is the middle of each screen edge; the corners are past 1. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen", meta = (EditCondition = "EdgeMode == EHadesWorldMarkerEdgeMode::Fade", EditConditionHides, ClampMin = "0"))
	float EdgeFadeStart = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen", meta = (EditCondition = "EdgeMode == EHadesWorldMarkerEdgeMode::Fade", EditConditionHides, ClampMin = "0"))
	float EdgeFadeEnd = 0.95f;

	/** Distance kept between the anchor point and each screen edge while clamped. Give a side more room when more of the marker's visible content is on that side of the anchor. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen")
	FMargin EdgePadding = FMargin(48.0f);

	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Visibility", meta = (ClampMin = "0", Units = "s"))
	float FadeInTime = 0.15f;

	/** Also how long a marker takes to fade out at the screen edge when its forced clamp ends. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Visibility", meta = (ClampMin = "0", Units = "s"))
	float FadeOutTime = 0.2f;

	/** Played forward when the marker appears and in reverse when it disappears, in place of the fades. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "World Marker|Visibility", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

	/** Shown while clamped, rotated around its own center to point at the anchor. Draw it as it looks on screen, pointing down. */
	UPROPERTY(BlueprintReadOnly, Category = "World Marker|Screen", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EdgeArrow;

	/** Keeps Edge Arrow visible while the marker is on screen, pointing down, instead of only while clamped. */
	UPROPERTY(EditDefaultsOnly, Category = "World Marker|Screen")
	bool bShowEdgeArrowOnScreen = false;

private:
	friend class UHadesWorldMarker;

	void ApplyOpacity();
	void ResetEdgeState();
	void UpdateClampRelease(float DeltaTime);

	TWeakObjectPtr<UHadesWorldMarker> Marker;

	float VisibilityAlpha = 0.0f;
	bool bShouldShow = false;
	float EdgeAlpha = 1.0f;
	bool bClampedToEdge = false;
	bool bForceClamp = false;
	bool bReleasingClamp = false;
};
