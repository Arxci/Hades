#include "ModularUIHoldTimer.h"
#include "ModularUIInput.h"
#include "Widgets/ModularKeyCapture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "InputAction.h"
#include "InputTriggers.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

BEGIN_DEFINE_SPEC(FModularUIInputSpec, "ModularUI.Input", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UInputAction> Action;
	FModularUIHoldTimer Timer;

	/** Gives the action a trigger of the class, and returns it. */
	template <typename TriggerType>
	TriggerType* AddTrigger()
	{
		TriggerType* Trigger = NewObject<TriggerType>(Action.Get());
		Action->Triggers.Add(Trigger);
		return Trigger;
	}

	static FString StepName(FModularUIHoldTimer::EStep Step)
	{
		switch (Step)
		{
		case FModularUIHoldTimer::EStep::Holding:
			return TEXT("Holding");
		case FModularUIHoldTimer::EStep::Completed:
			return TEXT("Completed");
		case FModularUIHoldTimer::EStep::Cancelled:
			return TEXT("Cancelled");
		default:
			return TEXT("None");
		}
	}

	static FString ResultName(EModularKeyCaptureResult Result)
	{
		switch (Result)
		{
		case EModularKeyCaptureResult::Bind:
			return TEXT("Bind");
		case EModularKeyCaptureResult::Cancel:
			return TEXT("Cancel");
		default:
			return TEXT("Ignore");
		}
	}

	/** Moves the hold on to Time, in seconds, and names what came of it, with its progress in Out Progress. */
	FString AdvanceTo(double Time, float& OutProgress)
	{
		return StepName(Timer.Advance(Time, OutProgress));
	}
END_DEFINE_SPEC(FModularUIInputSpec)

void FModularUIInputSpec::Define()
{
	BeforeEach([this]()
	{
		Action.Reset(NewObject<UInputAction>(GetTransientPackage()));
		Timer = FModularUIHoldTimer();
	});

	AfterEach([this]()
	{
		Action.Reset();
	});

	Describe("Hold time", [this]()
	{
		It("is the Hold Time Threshold of a Hold trigger on the input action", [this]()
		{
			AddTrigger<UInputTriggerPressed>();
			AddTrigger<UInputTriggerHold>()->HoldTimeThreshold = 0.8f;
			TestEqual(TEXT("The Hold trigger's threshold"), ModularUI::GetHoldTime(Action.Get()), 0.8f, 0.001f);
		});

		It("is 0 without a Hold trigger, for Hold And Release, and for no input action", [this]()
		{
			TestEqual(TEXT("No triggers"), ModularUI::GetHoldTime(Action.Get()), 0.0f, 0.001f);
			AddTrigger<UInputTriggerPressed>();
			TestEqual(TEXT("A Pressed trigger"), ModularUI::GetHoldTime(Action.Get()), 0.0f, 0.001f);
			AddTrigger<UInputTriggerHoldAndRelease>()->HoldTimeThreshold = 0.8f;
			TestEqual(TEXT("A Hold And Release trigger"), ModularUI::GetHoldTime(Action.Get()), 0.0f, 0.001f);
			TestEqual(TEXT("No input action"), ModularUI::GetHoldTime(nullptr), 0.0f, 0.001f);
		});
	});

	Describe("Binding arguments", [this]()
	{
		It("name the action by its Description, show it as asked, and run what they're given", [this]()
		{
			Action->ActionDescription = NSLOCTEXT("ModularUIInputSpec", "Organize", "Organize");
			bool bRan = false;
			const FBindUIActionArgs Args = ModularUI::MakeBindArgs(Action.Get(), false, FSimpleDelegate::CreateLambda([&bRan]() { bRan = true; }));
			TestTrue(TEXT("Binds the input action"), Args.InputAction.Get() == Action.Get());
			TestEqual(TEXT("Named by its Description"), Args.OverrideDisplayName.ToString(), FString(TEXT("Organize")));
			TestFalse(TEXT("Not shown in action bars"), Args.bDisplayInActionBar);

			Args.OnExecuteAction.ExecuteIfBound();
			TestTrue(TEXT("Runs what it was given"), bRan);
		});

		It("leave the name to Common UI without a Description", [this]()
		{
			const FBindUIActionArgs Args = ModularUI::MakeBindArgs(Action.Get(), true, FSimpleDelegate());
			TestTrue(TEXT("No name of its own"), Args.OverrideDisplayName.IsEmpty());
			TestTrue(TEXT("Shown in action bars"), Args.bDisplayInActionBar);
		});

		It("bind nothing for no input action", [this]()
		{
			TStrongObjectPtr<UBorder> Owner(NewObject<UBorder>(GetTransientPackage()));
			bool bRegistered = false;
			const FUIActionBindingHandle Handle = ModularUI::RegisterInputAction([&bRegistered](const FBindUIActionArgs&)
			{
				bRegistered = true;
				return FUIActionBindingHandle();
			}, *Owner, nullptr, true, FSimpleDelegate());
			TestFalse(TEXT("Nothing is registered"), bRegistered);
			TestFalse(TEXT("The handle binds nothing"), Handle.IsValid());
		});
	});

	Describe("Input methods", [this]()
	{
		It("give the keyboard's and mouse's keys to Mouse And Keyboard, a gamepad's to Gamepad, and none to Touch", [this]()
		{
			TestTrue(TEXT("A key on Mouse And Keyboard"), ModularUI::IsKeyForInputType(EKeys::R, ECommonInputType::MouseAndKeyboard));
			TestTrue(TEXT("A mouse button on Mouse And Keyboard"), ModularUI::IsKeyForInputType(EKeys::RightMouseButton, ECommonInputType::MouseAndKeyboard));
			TestFalse(TEXT("A key on Gamepad"), ModularUI::IsKeyForInputType(EKeys::R, ECommonInputType::Gamepad));
			TestTrue(TEXT("A face button on Gamepad"), ModularUI::IsKeyForInputType(EKeys::Gamepad_FaceButton_Bottom, ECommonInputType::Gamepad));
			TestFalse(TEXT("A face button on Mouse And Keyboard"), ModularUI::IsKeyForInputType(EKeys::Gamepad_FaceButton_Bottom, ECommonInputType::MouseAndKeyboard));
			TestFalse(TEXT("A key on Touch"), ModularUI::IsKeyForInputType(EKeys::R, ECommonInputType::Touch));
			TestFalse(TEXT("A touch on Mouse And Keyboard"), ModularUI::IsKeyForInputType(EKeys::TouchKeys[ETouchIndex::Touch1], ECommonInputType::MouseAndKeyboard));
			TestFalse(TEXT("No key"), ModularUI::IsKeyForInputType(FKey(), ECommonInputType::MouseAndKeyboard));
		});
	});

	Describe("Holds", [this]()
	{
		It("complete once the key has been held for the hold time, timed from its press", [this]()
		{
			Timer.Press(EKeys::E, 10.0);
			TestTrue(TEXT("Starts"), Timer.Start({ EKeys::E }, 1.0f));

			float Progress = 0.0f;
			TestEqual(TEXT("Holding a quarter second after the press"), AdvanceTo(10.25, Progress), FString(TEXT("Holding")));
			TestEqual(TEXT("A quarter of the way"), Progress, 0.25f, 0.001f);
			TestEqual(TEXT("Held long enough"), AdvanceTo(11.0, Progress), FString(TEXT("Completed")));
			TestEqual(TEXT("All the way"), Progress, 1.0f, 0.001f);
			TestFalse(TEXT("Over"), Timer.IsHolding());
			TestEqual(TEXT("Nothing more to move on"), AdvanceTo(11.5, Progress), FString(TEXT("None")));
		});

		It("cancel at 0 when the key is let go first", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Start({ EKeys::E }, 1.0f);
			AdvanceTo(0.5, Progress);
			Timer.Release(EKeys::E, 0.6);
			TestEqual(TEXT("Let go"), AdvanceTo(0.7, Progress), FString(TEXT("Cancelled")));
			TestEqual(TEXT("Empty at once"), Progress, 0.0f, 0.001f);
			TestFalse(TEXT("Over"), Timer.IsHolding());
		});

		It("count only the time the key was down when it was let go between two updates", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Start({ EKeys::E }, 1.0f);
			AdvanceTo(0.9, Progress);
			Timer.Release(EKeys::E, 1.05);
			TestEqual(TEXT("Let go after the hold time"), AdvanceTo(1.2, Progress), FString(TEXT("Completed")));

			Timer.Press(EKeys::E, 2.0);
			Timer.Start({ EKeys::E }, 1.0f);
			AdvanceTo(2.9, Progress);
			Timer.Release(EKeys::E, 2.95);
			TestEqual(TEXT("Let go just short of the hold time"), AdvanceTo(3.2, Progress), FString(TEXT("Cancelled")));
		});

		It("don't start without one of their keys down", [this]()
		{
			TestFalse(TEXT("Nothing down, such as for a click on an action bar button"), Timer.Start({ EKeys::E }, 1.0f));
			Timer.Press(EKeys::F, 0.0);
			TestFalse(TEXT("Another key down"), Timer.Start({ EKeys::E }, 1.0f));
			TestFalse(TEXT("Not holding"), Timer.IsHolding());
		});

		It("hold once per press, and again on the next press", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			TestTrue(TEXT("The press starts a hold"), Timer.Start({ EKeys::E }, 1.0f));
			TestFalse(TEXT("Starting again while it holds does nothing"), Timer.Start({ EKeys::E }, 1.0f));
			AdvanceTo(0.5, Progress);
			TestEqual(TEXT("Still timed from the press"), Progress, 0.5f, 0.001f);
			AdvanceTo(1.0, Progress);
			TestFalse(TEXT("The same press, still down, doesn't hold again"), Timer.Start({ EKeys::E }, 1.0f));

			Timer.Release(EKeys::E, 1.5);
			Timer.Press(EKeys::E, 2.0);
			TestTrue(TEXT("The next press holds"), Timer.Start({ EKeys::E }, 1.0f));
			AdvanceTo(2.5, Progress);
			TestEqual(TEXT("Timed from the next press"), Progress, 0.5f, 0.001f);
		});

		It("don't hold again for a press whose hold was cancelled", [this]()
		{
			Timer.Press(EKeys::E, 0.0);
			Timer.Start({ EKeys::E }, 1.0f);
			Timer.Cancel();
			TestFalse(TEXT("Cancelled"), Timer.IsHolding());
			TestFalse(TEXT("The same press doesn't start it again"), Timer.Start({ EKeys::E }, 1.0f));
		});

		It("keep holding while another of their keys is down", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Press(EKeys::Gamepad_FaceButton_Left, 0.2);
			Timer.Start({ EKeys::E, EKeys::Gamepad_FaceButton_Left }, 1.0f);
			Timer.Release(EKeys::E, 0.5);
			TestEqual(TEXT("Still holding"), AdvanceTo(0.6, Progress), FString(TEXT("Holding")));
			TestEqual(TEXT("Timed from the earliest press"), Progress, 0.6f, 0.001f);
			TestEqual(TEXT("Held long enough"), AdvanceTo(1.0, Progress), FString(TEXT("Completed")));
		});

		It("cancel when only keys that aren't theirs are still down", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Press(EKeys::F, 0.1);
			Timer.Start({ EKeys::E }, 1.0f);
			Timer.Release(EKeys::E, 0.3);
			TestEqual(TEXT("Let go"), AdvanceTo(0.4, Progress), FString(TEXT("Cancelled")));
			TestTrue(TEXT("The other key is still down"), Timer.HasKeysDown());
		});

		It("cancel when every key is let go at once, such as when the game loses focus", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Start({ EKeys::E }, 1.0f);
			Timer.ReleaseAll(0.5);
			TestFalse(TEXT("Nothing down"), Timer.HasKeysDown());
			TestEqual(TEXT("Let go"), AdvanceTo(0.6, Progress), FString(TEXT("Cancelled")));
		});

		It("cancel the hold in progress when another press starts one", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Start({ EKeys::E }, 1.0f);
			Timer.Press(EKeys::F, 0.3);
			TestTrue(TEXT("The new press starts its hold"), Timer.Start({ EKeys::F }, 1.0f));
			AdvanceTo(0.5, Progress);
			TestEqual(TEXT("Timed from the new press"), Progress, 0.2f, 0.001f);
		});

		It("complete on the first update when they take no time", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Start({ EKeys::E }, 0.0f);
			TestEqual(TEXT("Done at once"), AdvanceTo(0.0, Progress), FString(TEXT("Completed")));
			TestEqual(TEXT("All the way"), Progress, 1.0f, 0.001f);
		});

		It("time a key pressed again while down from its first press", [this]()
		{
			float Progress = 0.0f;
			Timer.Press(EKeys::E, 0.0);
			Timer.Press(EKeys::E, 0.5);
			Timer.Start({ EKeys::E }, 1.0f);
			AdvanceTo(0.5, Progress);
			TestEqual(TEXT("From the first press"), Progress, 0.5f, 0.001f);
		});
	});

	Describe("Waiting for a key to bind", [this]()
	{
		It("stops on a Cancel Key, whichever device the list is for", [this]()
		{
			const TArray<FKey> CancelKeys = { EKeys::Escape, EKeys::Gamepad_Special_Right };
			const TArray<FKey> NoKeys;
			TestEqual(TEXT("Escape, for keyboard and mouse"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::Escape, false, CancelKeys, NoKeys, false)), FString(TEXT("Cancel")));
			TestEqual(TEXT("Escape, for gamepad"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::Escape, true, CancelKeys, NoKeys, false)), FString(TEXT("Cancel")));
			TestEqual(TEXT("Start, for keyboard and mouse"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::Gamepad_Special_Right, false, CancelKeys, NoKeys, false)), FString(TEXT("Cancel")));
		});

		It("binds a key of the list's device, and ignores the other device's", [this]()
		{
			const TArray<FKey> NoKeys;
			TestEqual(TEXT("A key, for keyboard and mouse"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::R, false, NoKeys, NoKeys, false)), FString(TEXT("Bind")));
			TestEqual(TEXT("A key, for gamepad"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::R, true, NoKeys, NoKeys, false)), FString(TEXT("Ignore")));
			TestEqual(TEXT("A face button, for gamepad"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::Gamepad_FaceButton_Bottom, true, NoKeys, NoKeys, false)), FString(TEXT("Bind")));
			TestEqual(TEXT("A face button, for keyboard and mouse"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::Gamepad_FaceButton_Bottom, false, NoKeys, NoKeys, false)), FString(TEXT("Ignore")));
		});

		It("ignores Reserved Keys, touches, no key, and mouse buttons unless they're allowed", [this]()
		{
			const TArray<FKey> NoKeys;
			const TArray<FKey> ReservedKeys = { EKeys::F1 };
			TestEqual(TEXT("A reserved key"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::F1, false, NoKeys, ReservedKeys, false)), FString(TEXT("Ignore")));
			TestEqual(TEXT("A touch"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::TouchKeys[ETouchIndex::Touch1], false, NoKeys, NoKeys, true)), FString(TEXT("Ignore")));
			TestEqual(TEXT("No key"), ResultName(ModularUI::ClassifyCapturedKey(FKey(), false, NoKeys, NoKeys, true)), FString(TEXT("Ignore")));
			TestEqual(TEXT("A mouse button, not allowed"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::LeftMouseButton, false, NoKeys, NoKeys, false)), FString(TEXT("Ignore")));
			TestEqual(TEXT("A mouse button, allowed"), ResultName(ModularUI::ClassifyCapturedKey(EKeys::LeftMouseButton, false, NoKeys, NoKeys, true)), FString(TEXT("Bind")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
