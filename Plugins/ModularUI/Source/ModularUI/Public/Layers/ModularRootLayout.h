#pragma once

#include "CoreMinimal.h"

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "ModularRootLayout.generated.h"

class UCommonActivatableWidgetContainerBase;


/**
 * The widget a local player's whole UI is laid out in: a Common Activatable Widget Stack or Queue for each layer, one above the other, such
 * as Game for the HUD, Game Menu, Menu and Modal on top. Name each layer's stack in Layers with its tag, and widgets pushed onto that tag
 * with the Modular UI Subsystem go onto it. Set it as Root Layout Class in Project Settings > Plugins > Modular UI, and the subsystem makes
 * one for each player and keeps it on their screen. Lay anything else that should show over every layer in it too, such as message feeds.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class MODULARUI_API UModularRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Registers a layer that isn't in Layers, such as a stack made at runtime, with the owning player's Modular UI Subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Layers")
	void RegisterLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Layer);

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	//~ End UUserWidget Interface

	/** Each layer's tag, and the name of its Common Activatable Widget Stack or Queue in the designer, such as UI.Layer.Modal and ModalStack. */
	UPROPERTY(EditAnywhere, Category = "Layers", meta = (ForceInlineRow))
	TMap<FGameplayTag, FName> Layers;
};
