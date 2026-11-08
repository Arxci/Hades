#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"

#include "HadesItemBox.generated.h"

class UCommonActivatableWidget;
class UHadesInteractableComponent;
class UStaticMeshComponent;


/** Opens the item box screen. Every box shows the same storage, kept by the player controller, so what's stored in one is in all of them. */
UCLASS(Blueprintable)
class HADES_API AHadesItemBox : public AActor
{
	GENERATED_BODY()

public:
	AHadesItemBox();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Box")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item Box")
	TObjectPtr<UHadesInteractableComponent> Interactable;

	/** An inventory screen with an Item Box List. */
	UPROPERTY(EditAnywhere, Category = "Item Box")
	TSoftClassPtr<UCommonActivatableWidget> ScreenClass;

private:
	UFUNCTION()
	void HandleInteractionCompleted(AActor* Interactor);
};
