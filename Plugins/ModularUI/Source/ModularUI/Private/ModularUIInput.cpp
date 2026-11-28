#include "ModularUIInput.h"

#include "CommonInputSettings.h"
#include "CommonInputTypeEnum.h"
#include "CommonUITypes.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/IConsoleManager.h"
#include "ICommonInputModule.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBinding.h"
#include "InputAction.h"
#include "InputTriggers.h"
#include "PlayerMappableKeySettings.h"
#include "UObject/ObjectKey.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "ModularUI.h"
#include "ModularUIHoldSubsystem.h"

namespace ModularUI
{
	namespace
	{
		/** True the first time it's asked about the object for this set of warnings, so each warning is logged once per object. */
		bool IsFirstWarningFor(TSet<FObjectKey>& Warned, const UObject* Object)
		{
			bool bAlreadyWarned = false;
			Warned.Add(FObjectKey(Object), &bAlreadyWarned);
			return !bAlreadyWarned;
		}

		void WarnIfEnhancedInputSupportOff()
		{
			static bool bWarned = false;
			if (!bWarned && !IsEnhancedInputSupportEnabled())
			{
				bWarned = true;
				UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] Modular UI binds keys as Enhanced Input actions, which Common UI ignores while Enable Enhanced Input Support is off. Turn it on in Project Settings > Game > Common Input Settings."));
			}
		}

		void WarnIfNameless(const UInputAction& InputAction, bool bShouldDisplayInActionBar)
		{
			static TSet<FObjectKey> Warned;
			if (bShouldDisplayInActionBar && InputAction.ActionDescription.IsEmpty() && IsFirstWarningFor(Warned, &InputAction))
			{
				UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s shows in action bars without a name. Give it a Description."), *GetPathNameSafe(&InputAction));
			}
		}

		/** Common UI timing a hold of its own for the binding means the key would have to be held for both holds, one after the other. */
		void WarnIfCommonUIHolds(const FUIActionBindingHandle& Handle, const UInputAction& InputAction)
		{
			static TSet<FObjectKey> Warned;
			const TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle);
			if (Binding && Binding->HoldMappings.Num() > 0 && IsFirstWarningFor(Warned, &InputAction))
			{
				UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] Common UI times a hold of its own for %s, as well as Modular UI, so its key has to be held for both, one after the other. This engine version holds Enhanced Input actions itself: see Holds, under Input actions, in Modular UI's guide."),
					*GetPathNameSafe(&InputAction));
			}
		}

		/** The keys mapped for one input method, such as "R, Right Mouse Button", or "none". */
		FString DescribeKeys(const TArray<FKey>& Keys, ECommonInputType InputType)
		{
			TArray<FString> Names;
			for (const FKey& Key : Keys)
			{
				if (IsKeyForInputType(Key, InputType))
				{
					Names.Add(Key.GetDisplayName().ToString());
				}
			}
			return Names.IsEmpty() ? FString(TEXT("none")) : FString::Join(Names, TEXT(", "));
		}

		/** Whether the player can rebind the action: it has Player Mappable Key Settings, and a context mapping it is registered with Enhanced Input's user settings. */
		FString DescribeRebinding(const UInputAction& InputAction, UEnhancedInputUserSettings* UserSettings)
		{
			const UPlayerMappableKeySettings* KeySettings = InputAction.GetPlayerMappableKeySettings();
			if (!KeySettings)
			{
				return TEXT("not rebindable: it has no Player Mappable Key Settings");
			}
			if (!UserSettings)
			{
				return TEXT("not rebindable: Enhanced Input has no user settings, so turn on Enable User Settings");
			}
			for (const auto& Profile : UserSettings->GetAllAvailableKeyProfiles())
			{
				if (Profile.Value && Profile.Value->FindKeyMappingRow(KeySettings->Name))
				{
					return FString::Printf(TEXT("rebindable as %s"), *KeySettings->Name.ToString());
				}
			}
			return FString::Printf(TEXT("not rebindable yet: no context mapping %s is registered with Enhanced Input's user settings"), *KeySettings->Name.ToString());
		}

		void LogInputStatus(UWorld* World)
		{
			const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			if (!GameInstance)
			{
				UE_LOG(LogModularUI, Display, TEXT("[Modular UI] No game is running."));
				return;
			}

			UE_LOG(LogModularUI, Display, TEXT("[Modular UI] Common UI's Enhanced Input support is %s."), IsEnhancedInputSupportEnabled() ? TEXT("on") : TEXT("off, so input actions do nothing"));
			for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
			{
				UCommonUIActionRouterBase* Router = LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
				if (!Router)
				{
					continue;
				}
				UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				UEnhancedInputUserSettings* UserSettings = EnhancedInput ? EnhancedInput->GetUserSettings() : nullptr;

				const TArray<FUIActionBindingHandle> Bindings = Router->GatherActiveBindings();
				UE_LOG(LogModularUI, Display, TEXT("[Modular UI] %s takes %d actions now:"), *GetNameSafe(LocalPlayer), Bindings.Num());
				for (const FUIActionBindingHandle& Handle : Bindings)
				{
					const TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle);
					if (!Binding)
					{
						continue;
					}

					const FString WidgetName = GetNameSafe(Handle.GetBoundWidget());
					const FString Shown = Binding->bDisplayInActionBar ? FString::Printf(TEXT("shown as \"%s\""), *Handle.GetDisplayName().ToString()) : FString(TEXT("not shown"));
					const UInputAction* InputAction = Binding->InputAction.Get();
					if (!InputAction || !EnhancedInput)
					{
						UE_LOG(LogModularUI, Display, TEXT("    %s on %s: keys from its Common UI data table row or UI action tag; %s"), *Binding->ActionName.ToString(), *WidgetName, *Shown);
						continue;
					}

					const TArray<FKey> Keys = EnhancedInput->QueryKeysMappedToAction(InputAction);
					const float HoldSeconds = GetHoldTime(InputAction);
					const FString Hold = HoldSeconds > 0.0f ? FString::Printf(TEXT("a hold of %.2f s"), HoldSeconds) : FString(TEXT("runs as its key goes down"));
					UE_LOG(LogModularUI, Display, TEXT("    %s on %s: keyboard and mouse %s; gamepad %s; %s; %s; %s"), *InputAction->GetName(), *WidgetName,
						*DescribeKeys(Keys, ECommonInputType::MouseAndKeyboard), *DescribeKeys(Keys, ECommonInputType::Gamepad), *Shown, *Hold,
						*DescribeRebinding(*InputAction, UserSettings));
				}
			}
		}

		FAutoConsoleCommandWithWorld InputStatusCommand(
			TEXT("ModularUI.Input.Status"),
			TEXT("Logs each local player's UI actions that take input now: the widget that bound each, its keys on keyboard and mouse and on gamepad, its name in action bars, whether it's a hold, and whether players can rebind it."),
			FConsoleCommandWithWorldDelegate::CreateStatic(&LogInputStatus));
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

	bool IsKeyForInputType(const FKey& Key, ECommonInputType InputType)
	{
		if (!Key.IsValid() || Key.IsTouch() || InputType == ECommonInputType::Touch)
		{
			return false;
		}
		return Key.IsGamepadKey() == (InputType == ECommonInputType::Gamepad);
	}

	FBindUIActionArgs MakeBindArgs(const UInputAction* InputAction, bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction)
	{
		FBindUIActionArgs Args(InputAction, bShouldDisplayInActionBar, OnExecuteAction);
		if (InputAction && !InputAction->ActionDescription.IsEmpty())
		{
			Args.OverrideDisplayName = InputAction->ActionDescription;
		}
		return Args;
	}

	FUIActionBindingHandle RegisterInputAction(TFunctionRef<FUIActionBindingHandle(const FBindUIActionArgs&)> Register, const UWidget& Owner,
		const UInputAction* InputAction, bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction)
	{
		if (!InputAction)
		{
			return FUIActionBindingHandle();
		}
		WarnIfEnhancedInputSupportOff();
		WarnIfNameless(*InputAction, bShouldDisplayInActionBar);

		FUIActionBindingHandle Handle;
		const float HoldSeconds = GetHoldTime(InputAction);
		if (HoldSeconds <= 0.0f)
		{
			Handle = Register(MakeBindArgs(InputAction, bShouldDisplayInActionBar, OnExecuteAction));
		}
		else
		{
			// Common UI runs an Enhanced Input action as its key goes down, whatever its triggers, so the press starts a hold of Modular UI's
			// own. The handle is only known once the binding is made, so the press finds it through this.
			const TSharedRef<FUIActionBindingHandle> SharedHandle = MakeShared<FUIActionBindingHandle>();
			const TWeakObjectPtr<const UWidget> WeakOwner(&Owner);
			const TWeakObjectPtr<const UInputAction> WeakAction(InputAction);
			*SharedHandle = Register(MakeBindArgs(InputAction, bShouldDisplayInActionBar, FSimpleDelegate::CreateLambda([SharedHandle, WeakOwner, WeakAction, HoldSeconds, OnExecuteAction]()
			{
				const UWidget* Widget = WeakOwner.Get();
				const UInputAction* Action = WeakAction.Get();
				if (UModularUIHoldSubsystem* Holds = Widget && Action ? UModularUIHoldSubsystem::Get(*Widget) : nullptr)
				{
					Holds->StartHold(*Action, *SharedHandle, HoldSeconds, OnExecuteAction);
				}
			})));
			Handle = *SharedHandle;
			WarnIfCommonUIHolds(Handle, *InputAction);
		}

		// Once per action: widgets that bind an action as the player's mode changes would otherwise repeat it each time.
		static TSet<FObjectKey> WarnedUnbound;
		if (!Handle.IsValid() && IsEnhancedInputSupportEnabled() && IsFirstWarningFor(WarnedUnbound, InputAction))
		{
			UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] Common UI didn't bind %s on %s, so its key does nothing there. A widget binds each input action once, and only while it has an owning local player."),
				*GetPathNameSafe(InputAction), *GetPathNameSafe(&Owner));
		}
		return Handle;
	}

	const UInputAction* GetAcceptInputAction()
	{
		return IsEnhancedInputSupportEnabled() ? ICommonInputModule::GetSettings().GetEnhancedInputClickAction() : nullptr;
	}

	bool HasAcceptAction()
	{
		return GetAcceptInputAction() != nullptr || !ICommonInputModule::GetSettings().GetDefaultClickAction().IsNull();
	}

	FBindUIActionArgs MakeAcceptBindArgs(bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction)
	{
		if (const UInputAction* Accept = GetAcceptInputAction())
		{
			return FBindUIActionArgs(Accept, bShouldDisplayInActionBar, OnExecuteAction);
		}
		return FBindUIActionArgs(ICommonInputModule::GetSettings().GetDefaultClickAction(), bShouldDisplayInActionBar, OnExecuteAction);
	}
}
