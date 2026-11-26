#include "Widgets/ModularInputKeyWidget.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Image.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"

void UModularInputKeyWidget::SetInputAction(const UInputAction* InInputAction)
{
	InputAction = InInputAction;
	Refresh();
}

void UModularInputKeyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The key changes with the device in use, and with the player's rebinding, which rebuilds the control mappings.
	if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		CommonInput->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleControlMappingsRebuilt);
	}
	Refresh();
}

void UModularInputKeyWidget::NativeDestruct()
{
	if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		CommonInput->OnInputMethodChangedNative.RemoveAll(this);
	}
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput())
	{
		EnhancedInput->ControlMappingsRebuiltDelegate.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UModularInputKeyWidget::HandleControlMappingsRebuilt()
{
	Refresh();
}

void UModularInputKeyWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	Refresh();
}

UEnhancedInputLocalPlayerSubsystem* UModularInputKeyWidget::GetEnhancedInput() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

void UModularInputKeyWidget::Refresh()
{
	UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = GetEnhancedInput();
	const ECommonInputType InputType = CommonInput ? CommonInput->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard;

	// The first key bound for the device in use: a gamepad's own keys, or else the keyboard's and the mouse's. A touch screen has none.
	FKey Key;
	if (InputAction && EnhancedInput && InputType != ECommonInputType::Touch)
	{
		const bool bGamepad = InputType == ECommonInputType::Gamepad;
		for (const FKey& Mapped : EnhancedInput->QueryKeysMappedToAction(InputAction))
		{
			if (Mapped.IsValid() && !Mapped.IsTouch() && Mapped.IsGamepadKey() == bGamepad)
			{
				Key = Mapped;
				break;
			}
		}
	}

	FSlateBrush Brush;
	const bool bHasIcon = Key.IsValid() && CommonInput
		&& UCommonInputPlatformSettings::Get()->TryGetInputBrush(Brush, Key, InputType, CommonInput->GetCurrentGamepadName());
	if (bHasIcon && KeyIcon)
	{
		KeyIcon->SetBrush(Brush);
	}

	// Collapsed without an icon, as Common Action Widget is, so whatever is laid out around it closes up. Never hit-testable, so it
	// doesn't take the mouse from the button or item it's on.
	SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	const FKey Shown = bHasIcon ? Key : FKey();
	if (Shown != ShownKey)
	{
		ShownKey = Shown;
		OnKeyChanged(ShownKey);
	}
}
