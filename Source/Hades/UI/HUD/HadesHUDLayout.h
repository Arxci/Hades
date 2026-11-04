#pragma once

#include "CoreMinimal.h"

#include "Hades/UI/Foundation/HadesActivatableWidget.h"

#include "HadesHUDLayout.generated.h"

class UCommonActivatableWidget;

/** The player's HUD, pushed onto the Game layer by the player controller. Escape opens EscapeMenuClass on the Menu layer. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class HADES_API UHadesHUDLayout : public UHadesActivatableWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSoftClassPtr<UCommonActivatableWidget> EscapeMenuClass;

private:
	void HandleEscapeAction();
};
