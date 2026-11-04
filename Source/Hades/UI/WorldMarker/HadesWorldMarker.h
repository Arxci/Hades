#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "UObject/Object.h"

#include "HadesWorldMarker.generated.h"

class USceneComponent;
class UHadesWorldMarkerManagerComponent;
class UHadesWorldMarkerWidget;

/**
 * Something to point out in the world. Add it to the player's UHadesWorldMarkerManagerComponent and the HUD's
 * UHadesWorldMarkerLayer draws its widget over the anchor. Its owner drives the widget with state tags and progress.
 */
UCLASS(BlueprintType)
class HADES_API UHadesWorldMarker : public UObject
{
	GENERATED_BODY()

public:
	// Setup
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetAnchor(USceneComponent* InAnchor, FName InSocketName = NAME_None);

	UFUNCTION(BlueprintPure, Category = "World Marker")
	USceneComponent* GetAnchor() const { return Anchor.Get(); }

	FVector GetAnchorLocation() const;

	/** Loaded in the background the first time a layer needs it. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetWidgetClass(TSoftClassPtr<UHadesWorldMarkerWidget> InWidgetClass) { WidgetClass = InWidgetClass; }

	const TSoftClassPtr<UHadesWorldMarkerWidget>& GetWidgetClass() const { return WidgetClass; }

	/** What the marker represents, such as an interactable component. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetContextObject(UObject* InContextObject) { ContextObject = InContextObject; }

	UFUNCTION(BlueprintPure, Category = "World Marker")
	UObject* GetContextObject() const { return ContextObject.Get(); }

	/** Higher priorities draw on top. Within a priority, nearer markers draw on top. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetPriority(int32 InPriority) { Priority = InPriority; }

	int32 GetPriority() const { return Priority; }

	/** Hidden markers stay registered and fade out. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

	UFUNCTION(BlueprintPure, Category = "World Marker")
	bool IsVisible() const { return bVisible; }

	// State
	UFUNCTION(BlueprintCallable, Category = "World Marker|State")
	void SetStateTag(FGameplayTag Tag, bool bEnabled);

	/** 0 to 1. */
	UFUNCTION(BlueprintCallable, Category = "World Marker|State")
	void SetProgress(float NewProgress);

	UFUNCTION(BlueprintPure, Category = "World Marker|State")
	bool HasStateTag(FGameplayTag Tag) const { return StateTags.HasTagExact(Tag); }

	UFUNCTION(BlueprintPure, Category = "World Marker|State")
	float GetProgress() const { return Progress; }

	// Events
	/** A one-shot moment, such as an interaction completing. Dropped when no widget is showing the marker. */
	UFUNCTION(BlueprintCallable, Category = "World Marker|Events")
	void NotifyEvent(FGameplayTag EventTag);

	/** Call when the context object's data changes, so the widget can redraw. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void RefreshWidget();

	// Registration
	void Unregister();

private:
	friend class UHadesWorldMarkerManagerComponent;
	friend class UHadesWorldMarkerLayer;

	TWeakObjectPtr<USceneComponent> Anchor;
	FName SocketName;

	UPROPERTY()
	TSoftClassPtr<UHadesWorldMarkerWidget> WidgetClass;

	TWeakObjectPtr<UObject> ContextObject;
	int32 Priority = 0;
	bool bVisible = true;

	FGameplayTagContainer StateTags;
	float Progress = 0.0f;

	TWeakObjectPtr<UHadesWorldMarkerManagerComponent> Manager;

	/** Set by the layer while a widget is drawing this marker. */
	TWeakObjectPtr<UHadesWorldMarkerWidget> Widget;
};
