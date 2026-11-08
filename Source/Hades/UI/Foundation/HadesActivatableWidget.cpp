#include "HadesActivatableWidget.h"

#include "Editor/WidgetCompilerLog.h"

#include "Hades/Player/HadesPlayerController.h"

#define LOCTEXT_NAMESPACE "Hades"

TOptional<FUIInputConfig> UHadesActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case EHadesWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EHadesWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EHadesWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EHadesWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

void UHadesActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer());
	if (bPauseGame && Controller)
	{
		Controller->AddPauseRequest(this);
	}
}

void UHadesActivatableWidget::NativeOnDeactivated()
{
	AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer());
	if (bPauseGame && Controller)
	{
		Controller->RemovePauseRequest(this);
	}

	Super::NativeOnDeactivated();
}

#if WITH_EDITOR
void UHadesActivatableWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	if (!GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UHadesActivatableWidget, BP_GetDesiredFocusTarget)))
	{
		if (GetParentNativeClass(GetClass()) == UHadesActivatableWidget::StaticClass())
		{
			CompileLog.Warning(LOCTEXT("ValidateGetDesiredFocusTarget_Warning", "GetDesiredFocusTarget wasn't implemented, you're going to have trouble using gamepads on this screen."));
		}
		else
		{
			CompileLog.Note(LOCTEXT("ValidateGetDesiredFocusTarget_Note", "GetDesiredFocusTarget wasn't implemented, you're going to have trouble using gamepads on this screen. If it was implemented in the native base class you can ignore this message."));
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
