#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "UObject/Object.h"

#include "ModularWorldMarker.generated.h"

class USceneComponent;
class UModularWorldMarkerManagerComponent;
class UModularWorldMarkerWidget;


/**
 * Something to point out in the world, such as an interactable, an objective or an enemy. Add it to the player's Modular World Marker
 * Manager Component and the HUD's Modular World Marker Layer draws its widget over the anchor. Its owner drives the widget with state tags,
 * progress and one-shot events, so the widget never needs to know the game's classes.
 */
UCLASS(BlueprintType)
class MODULARUI_API UModularWorldMarker : public UObject
{
	GENERATED_BODY()

public:
	// Setup
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetAnchor(USceneComponent* InAnchor, FName InSocketName = NAME_None);

	UFUNCTION(BlueprintPure, Category = "World Marker")
	USceneComponent* GetAnchor() const { return Anchor.Get(); }

	/** Where the marker points: the anchor's socket, moved by World Offset. */
	FVector GetAnchorLocation() const;

	/** Moves the point the marker draws at from its anchor, in world space, such as up above an item lying on the floor. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetWorldOffset(FVector InWorldOffset) { WorldOffset = InWorldOffset; }

	UFUNCTION(BlueprintPure, Category = "World Marker")
	FVector GetWorldOffset() const { return WorldOffset; }

	/** Loaded in the background the first time a layer needs it. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetWidgetClass(TSoftClassPtr<UModularWorldMarkerWidget> InWidgetClass) { WidgetClass = InWidgetClass; }

	const TSoftClassPtr<UModularWorldMarkerWidget>& GetWidgetClass() const { return WidgetClass; }

	/** What the marker represents, such as an interactable component, for its widget to read. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetContextObject(UObject* InContextObject) { ContextObject = InContextObject; }

	UFUNCTION(BlueprintPure, Category = "World Marker")
	UObject* GetContextObject() const { return ContextObject.Get(); }

	/** Higher priorities draw on top. Within a priority, nearer markers draw on top. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetPriority(int32 InPriority) { Priority = InPriority; }

	UFUNCTION(BlueprintPure, Category = "World Marker")
	int32 GetPriority() const { return Priority; }

	/** Hidden markers stay registered and fade out. */
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

	UFUNCTION(BlueprintPure, Category = "World Marker")
	bool IsVisible() const { return bVisible; }

	// State
	UFUNCTION(BlueprintCallable, Category = "World Marker|State")
	void SetStateTag(FGameplayTag Tag, bool bEnabled);

	/** 0 to 1, such as how long an interaction has been held. */
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
	UFUNCTION(BlueprintCallable, Category = "World Marker")
	void Unregister();

private:
	friend class UModularWorldMarkerManagerComponent;
	friend class UModularWorldMarkerLayer;

	TWeakObjectPtr<USceneComponent> Anchor;
	FName SocketName;
	FVector WorldOffset = FVector::ZeroVector;

	UPROPERTY()
	TSoftClassPtr<UModularWorldMarkerWidget> WidgetClass;

	TWeakObjectPtr<UObject> ContextObject;
	int32 Priority = 0;
	bool bVisible = true;

	FGameplayTagContainer StateTags;
	float Progress = 0.0f;

	TWeakObjectPtr<UModularWorldMarkerManagerComponent> Manager;

	/** Set by the layer while a widget is drawing this marker. */
	TWeakObjectPtr<UModularWorldMarkerWidget> Widget;
};
