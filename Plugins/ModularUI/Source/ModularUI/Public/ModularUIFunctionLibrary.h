#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Messages/ModularMessageTypes.h"

#include "ModularUIFunctionLibrary.generated.h"

class APlayerController;
class UCommonActivatableWidget;
class UImage;
class USizeBox;
class UWidget;


/** Modular UI's helpers for Blueprint: layers and messages from a player controller, and small widget utilities. */
UCLASS()
class MODULARUI_API UModularUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Layers
	/** Pushes a widget of the class onto the player's layer, such as UI.Layer.Menu, and returns it. Null when the player has no such layer. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Modular UI|Layers", meta = (DeterminesOutputType = "WidgetClass"))
	static UCommonActivatableWidget* PushWidgetToLayer(APlayerController* PlayerController, UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag,
		UPARAM(meta = (AllowAbstract = false)) TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Loads the class in the background, then pushes a widget of it onto the player's layer. The player's input waits meanwhile. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Modular UI|Layers")
	static void PushStreamedWidgetToLayer(APlayerController* PlayerController, UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag,
		UPARAM(meta = (AllowAbstract = false)) TSoftClassPtr<UCommonActivatableWidget> WidgetClass);

	// Messages
	/** Shows a message to the player, in every feed showing its channel. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Modular UI|Messages")
	static void ShowMessage(APlayerController* PlayerController, const FModularMessage& Message);

	/** Shows a line of text to the player on a channel, such as UI.Message.Hint. Duration below 0 uses each feed's. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Modular UI|Messages")
	static void ShowMessageText(APlayerController* PlayerController, UPARAM(meta = (Categories = "UI.Message")) FGameplayTag Channel, const FText& Text, float Duration = -1.0f);

	/** Hides the player's message with the key on the channel. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Modular UI|Messages")
	static void HideMessage(APlayerController* PlayerController, UPARAM(meta = (Categories = "UI.Message")) FGameplayTag Channel, FName Key);

	// Widgets
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Widgets")
	static void SetSizeBoxSize(USizeBox* Target, const FVector2D NewSize);

	/** Sets the Image Size of the image's brush, such as to size an icon from code. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Widgets")
	static void SetImageBrushSize(UImage* Target, const FVector2D NewSize);

	/**
	 * Moves the widget's render opacity toward Target Opacity at Interp Speed, by the world's delta time: call it each frame, such as from
	 * Tick. It waits with the world while the game is paused.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Widgets")
	static void InterpolateWidgetRenderOpacity(UWidget* Target, const float TargetOpacity, const float InterpSpeed);
};
