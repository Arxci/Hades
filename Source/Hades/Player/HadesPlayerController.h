#pragma once

#include "CoreMinimal.h"

#include "CommonPlayerController.h"

#include "HadesPlayerController.generated.h"

class UCommonActivatableWidget;
class UInputMappingContext;
class UHadesWorldMarkerManagerComponent;


UCLASS()
class HADES_API AHadesPlayerController : public ACommonPlayerController
{
	GENERATED_BODY()
	
public:
	AHadesPlayerController();
	
	virtual void ReceivedPlayer() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;
	
	UPROPERTY(VisibleAnywhere, Category = "UI")
	TObjectPtr<UHadesWorldMarkerManagerComponent> WorldMarkerManager;

	/** Pushed onto the Game layer for the local player. Place a World Marker Layer in it to draw world markers. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSoftClassPtr<UCommonActivatableWidget> HUDLayoutClass;

private:
	TWeakObjectPtr<UCommonActivatableWidget> HUDLayout;
};