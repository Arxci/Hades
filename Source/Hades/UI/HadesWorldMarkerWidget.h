#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "HadesWorldMarkerWidget.generated.h"

class AActor;
class USceneComponent;

/** A widget drawn over something in the world. Its owner drives it with state tags, progress and one-shot event tags; children react in the Blueprint events below. */
UCLASS(Abstract)
class HADES_API UHadesWorldMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Context
	void SetContext(USceneComponent* InAnchor, AActor* InContextActor);

	UFUNCTION(BlueprintPure, Category = "World Marker|Context")
	USceneComponent* GetAnchor() const { return Anchor.Get(); }

	UFUNCTION(BlueprintPure, Category = "World Marker|Context")
	AActor* GetContextActor() const { return ContextActor.Get(); }

	// State
	void SetStateTag(const FGameplayTag& Tag, bool bEnabled);
	void SetProgress(float NewProgress);

	UFUNCTION(BlueprintPure, Category = "World Marker|State")
	bool HasStateTag(FGameplayTag Tag) const { return StateTags.HasTagExact(Tag); }

	UFUNCTION(BlueprintPure, Category = "World Marker|State")
	float GetProgress() const { return Progress; }

	// Events
	void NotifyEvent(const FGameplayTag& EventTag);

protected:
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

private:
	TWeakObjectPtr<USceneComponent> Anchor;
	TWeakObjectPtr<AActor> ContextActor;

	FGameplayTagContainer StateTags;
	float Progress = 0.0f;
};
