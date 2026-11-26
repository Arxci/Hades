#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"

#include "ModularInputKeyWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UImage;
class UInputAction;
enum class ECommonInputType : uint8;


/**
 * The icon of the key the player has bound to an Enhanced Input action, for the device in use, from the platform's Common Input controller
 * data. It follows the player's rebinding, and switches between keyboard and gamepad keys as the device changes. Use it for any prompt that
 * should show the player's own key, such as on a world marker, a quick slot or a message.
 * Collapsed, as Common Action Widget is, whenever there's no icon to show: without an action, on a touch screen, while no key is bound for
 * the device, or for a key the controller data has no icon for.
 */
UCLASS(Abstract)
class MODULARUI_API UModularInputKeyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Input Key")
	void SetInputAction(const UInputAction* InInputAction);

	UFUNCTION(BlueprintPure, Category = "Input Key")
	const UInputAction* GetInputAction() const { return InputAction; }

	/** The key shown, or none. */
	UFUNCTION(BlueprintPure, Category = "Input Key")
	FKey GetShownKey() const { return ShownKey; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The key shown changed, such as to resize a badge around it. None while the widget is collapsed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Input Key")
	void OnKeyChanged(FKey Key);

	/**
	 * Shows the key's icon: the key's brush from the controller data, Image Size included. To show it at another size where it's placed,
	 * put that placed widget in a Size Box.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Input Key", meta = (BindWidget))
	TObjectPtr<UImage> KeyIcon;

	/** The action shown until Set Input Action is called, such as for a prompt that's always the same. */
	UPROPERTY(EditAnywhere, Category = "Input Key")
	TObjectPtr<const UInputAction> InputAction;

private:
	UFUNCTION()
	void HandleControlMappingsRebuilt();

	void HandleInputMethodChanged(ECommonInputType InputType);
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInput() const;
	void Refresh();

	FKey ShownKey;
};
