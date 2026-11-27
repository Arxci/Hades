#include "ModularUIInput.h"

#include "CommonInputSettings.h"
#include "CommonUITypes.h"
#include "Components/Widget.h"
#include "ICommonInputModule.h"
#include "InputAction.h"
#include "InputTriggers.h"

#include "ModularUI.h"
#include "ModularUIHoldSubsystem.h"

namespace ModularUI
{
	namespace
	{
		void WarnIfEnhancedInputSupportOff()
		{
			static bool bWarned = false;
			if (!bWarned && !IsEnhancedInputSupportEnabled())
			{
				bWarned = true;
				UE_LOG(LogModularUI, Warning, TEXT("Modular UI binds keys as Enhanced Input actions, which Common UI ignores while Enable Enhanced Input Support is off. Turn it on in Project Settings > Game > Common Input Settings."));
			}
		}
	}

	bool IsEnhancedInputSupportEnabled()
	{
		return CommonUI::IsEnhancedInputSupportEnabled();
	}

	float GetHoldTime(const UInputAction* InputAction)
	{
		if (InputAction)
		{
			for (const UInputTrigger* Trigger : InputAction->Triggers)
			{
				if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger))
				{
					return FMath::Max(Hold->HoldTimeThreshold, 0.0f);
				}
			}
		}
		return 0.0f;
	}

	FBindUIActionArgs MakeBindArgs(const UInputAction* InputAction, bool bDisplayInActionBar, const FSimpleDelegate& OnExecuteAction)
	{
		WarnIfEnhancedInputSupportOff();
		FBindUIActionArgs Args(InputAction, bDisplayInActionBar, OnExecuteAction);
		if (InputAction && !InputAction->ActionDescription.IsEmpty())
		{
			Args.OverrideDisplayName = InputAction->ActionDescription;
		}
		return Args;
	}

	FUIActionBindingHandle RegisterInputAction(TFunctionRef<FUIActionBindingHandle(const FBindUIActionArgs&)> Register, const UWidget& Owner,
		const UInputAction* InputAction, bool bDisplayInActionBar, const FSimpleDelegate& OnExecuteAction)
	{
		if (!InputAction)
		{
			return FUIActionBindingHandle();
		}

		const float HoldTime = GetHoldTime(InputAction);
		if (HoldTime <= 0.0f)
		{
			return Register(MakeBindArgs(InputAction, bDisplayInActionBar, OnExecuteAction));
		}

		// Common UI runs an Enhanced Input action as its key goes down, whatever its triggers, so the press starts a hold of Modular UI's
		// own. The handle is only known once the binding is made, so the press finds it through this.
		const TSharedRef<FUIActionBindingHandle> Handle = MakeShared<FUIActionBindingHandle>();
		const TWeakObjectPtr<const UWidget> WeakOwner(&Owner);
		const TWeakObjectPtr<const UInputAction> WeakAction(InputAction);
		*Handle = Register(MakeBindArgs(InputAction, bDisplayInActionBar, FSimpleDelegate::CreateLambda([Handle, WeakOwner, WeakAction, HoldTime, OnExecuteAction]()
		{
			const UWidget* Widget = WeakOwner.Get();
			const UInputAction* Action = WeakAction.Get();
			if (UModularUIHoldSubsystem* Holds = Widget && Action ? UModularUIHoldSubsystem::Get(*Widget) : nullptr)
			{
				Holds->StartHold(*Widget, *Action, *Handle, HoldTime, OnExecuteAction);
			}
		})));
		return *Handle;
	}

	const UInputAction* GetAcceptInputAction()
	{
		return IsEnhancedInputSupportEnabled() ? ICommonInputModule::GetSettings().GetEnhancedInputClickAction() : nullptr;
	}

	bool HasAcceptAction()
	{
		return GetAcceptInputAction() != nullptr || !ICommonInputModule::GetSettings().GetDefaultClickAction().IsNull();
	}

	FBindUIActionArgs MakeAcceptBindArgs(bool bDisplayInActionBar, const FSimpleDelegate& OnExecuteAction)
	{
		if (const UInputAction* Accept = GetAcceptInputAction())
		{
			return FBindUIActionArgs(Accept, bDisplayInActionBar, OnExecuteAction);
		}
		return FBindUIActionArgs(ICommonInputModule::GetSettings().GetDefaultClickAction(), bDisplayInActionBar, OnExecuteAction);
	}
}
