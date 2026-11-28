#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"

#include "ModularInputKeyWidget.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UImage;
class UInputAction;
enum class ECommonInputType : uint8;


/** Which devices a Modular Input Key Widget shows a key for. */
UENUM(BlueprintType)
enum class EModularInputKeyShowOn : uint8
{
	/** Whichever is in use: the keyboard's or the mouse's key, or a gamepad's. */
	AnyDevice,
	/** Only while a gamepad is in use, such as on a menu's buttons, which the mouse clicks and the keyboard reaches through focus. */
	Gamepad,
	/** Only while the keyboard and mouse are in use. */
	KeyboardAndMouse
};


/**
 * The icon of the key the player has bound to an Enhanced Input action, for the device in use, from the platform's Common Input controller
 * data. It follows the player's rebinding, and switches between keyboard and gamepad keys as the device changes. Use it for any prompt that
 * should show the player's own key, such as on a world marker, a quick slot, a message or a tab list's Next Tab and Previous Tab. A Modular
 * Button shows its own key in one named Key Widget.
 * Collapsed, as Common Action Widget is, whenever there's no icon to show: without an action, on a touch screen, on a device Show On leaves
 * out, while no key is bound for the device, or for a key the controller data has no icon for. In the designer it stays as laid out, so it
 * can be seen and placed.
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

	UFUNCTION(BlueprintCallable, Category = "Input Key")
	void SetShowOn(EModularInputKeyShowOn InShowOn);

	UFUNCTION(BlueprintPure, Category = "Input Key")
	EModularInputKeyShowOn GetShowOn() const { return ShowOn; }

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

	/** Which devices it shows a key for. On any other, it's collapsed. */
	UPROPERTY(EditAnywhere, Category = "Input Key")
	EModularInputKeyShowOn ShowOn = EModularInputKeyShowOn::AnyDevice;

private:
	UFUNCTION()
	void HandleControlMappingsRebuilt();

	void HandleInputMethodChanged(ECommonInputType InputType);
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedInput() const;
	void Refresh();

	FKey ShownKey;
};
