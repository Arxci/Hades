#include "ModularUIHoldSubsystem.h"

#include "Components/Widget.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Input/UIActionBinding.h"
#include "InputAction.h"

/**
 * Watches which keys are down, and when each went down and came up, without taking any input, and times the hold in progress from the core
 * ticker, which runs with the game paused too. Knowing when a key went down from before the hold starts, it times a hold from the press itself,
 * and knowing when the last key came up, a key let go just short of the time never completes it.
 */
class FModularUIHoldProcessor final : public IInputProcessor
{
public:
	virtual ~FModularUIHoldProcessor() override
	{
		StopTicker();
	}

	void Start(TArray<FKey>&& InKeys, FUIActionBindingHandle InBinding, float InHoldTime, const FSimpleDelegate& InOnComplete)
	{
		Cancel();

		// Timed from the earliest of its keys still down. With none down, nothing holds it, such as for a click on its action bar button.
		double PressTime = 0.0;
		bool bAnyDown = false;
		for (const FKey& Key : InKeys)
		{
			if (const double* Found = PressedAt.Find(Key))
			{
				PressTime = bAnyDown ? FMath::Min(PressTime, *Found) : *Found;
				bAnyDown = true;
			}
		}
		if (!bAnyDown)
		{
			return;
		}

		bHolding = true;
		bLetGo = false;
		Keys = MoveTemp(InKeys);
		Binding = InBinding;
		HoldTime = InHoldTime;
		StartTime = PressTime;
		OnComplete = InOnComplete;
		StartTicker();
	}

	/** Ends the hold in progress without running it. */
	void Cancel()
	{
		if (bHolding)
		{
			End(/*bCompleted*/ false);
		}
	}

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		// Keys let go while the game is in the background never say so.
		if (!SlateApp.IsActive() && PressedAt.Num() > 0)
		{
			TArray<FKey> Down;
			PressedAt.GetKeys(Down);
			for (const FKey& Key : Down)
			{
				Release(Key);
			}
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (!InKeyEvent.IsRepeat())
		{
			Press(InKeyEvent.GetKey());
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Release(InKeyEvent.GetKey());
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Press(MouseEvent.GetEffectingButton());
		return false;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Release(MouseEvent.GetEffectingButton());
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("ModularUIHold"); }
	//~ End IInputProcessor Interface

private:
	void Press(const FKey& Key)
	{
		if (Key.IsValid() && !PressedAt.Contains(Key))
		{
			PressedAt.Add(Key, FPlatformTime::Seconds());
		}
	}

	void Release(const FKey& Key)
	{
		PressedAt.Remove(Key);
		if (bHolding && !bLetGo && Keys.Contains(Key) && !IsAnyKeyDown())
		{
			bLetGo = true;
			LetGoTime = FPlatformTime::Seconds();
		}
	}

	bool IsAnyKeyDown() const
	{
		return Keys.ContainsByPredicate([this](const FKey& Key) { return PressedAt.Contains(Key); });
	}

	void StartTicker()
	{
		if (!TickerHandle.IsValid())
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FModularUIHoldProcessor::TickHold));
		}
	}

	void StopTicker()
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle = FTSTicker::FDelegateHandle();
		}
	}

	bool TickHold(float DeltaTime)
	{
		Advance();
		if (!bHolding)
		{
			TickerHandle = FTSTicker::FDelegateHandle();
			return false;
		}
		return true;
	}

	/** Moves the hold on: runs it once it was held long enough, ends it once it was let go short of that, and otherwise shows how far it is. */
	void Advance()
	{
		if (!bHolding)
		{
			return;
		}

		// Its binding goes when its widget unbinds it, such as as the widget closes, and the hold with it.
		if (!Binding.IsValid())
		{
			End(/*bCompleted*/ false);
			return;
		}

		const double HeldUntil = bLetGo ? LetGoTime : FPlatformTime::Seconds();
		const float Progress = HoldTime > 0.0f ? FMath::Clamp(static_cast<float>((HeldUntil - StartTime) / HoldTime), 0.0f, 1.0f) : 1.0f;
		if (Progress >= 1.0f)
		{
			End(/*bCompleted*/ true);
		}
		else if (bLetGo)
		{
			End(/*bCompleted*/ false);
		}
		else
		{
			ShowProgress(Progress);
		}
	}

	void ShowProgress(float Progress) const
	{
		if (const TSharedPtr<FUIActionBinding> Found = FUIActionBinding::FindBinding(Binding))
		{
			Found->OnHoldActionProgressed.Broadcast(Progress);
		}
	}

	void End(bool bCompleted)
	{
		// The progress drops to 0 at once either way, and the hold is over before the action runs, so the action can start another.
		ShowProgress(0.0f);
		const FSimpleDelegate Completed = OnComplete;
		bHolding = false;
		bLetGo = false;
		Keys.Reset();
		Binding = FUIActionBindingHandle();
		OnComplete.Unbind();
		if (bCompleted)
		{
			Completed.ExecuteIfBound();
		}
	}

	/** When each key that's down now went down, in platform seconds. */
	TMap<FKey, double> PressedAt;

	// The hold in progress: the keys mapped to its action, its binding, how long it takes, when its key went down and, once every key is let
	// go, when that was, and what it runs.
	bool bHolding = false;
	bool bLetGo = false;
	TArray<FKey> Keys;
	FUIActionBindingHandle Binding;
	float HoldTime = 0.0f;
	double StartTime = 0.0;
	double LetGoTime = 0.0;
	FSimpleDelegate OnComplete;

	FTSTicker::FDelegateHandle TickerHandle;
};

UModularUIHoldSubsystem* UModularUIHoldSubsystem::Get(const UWidget& Widget)
{
	const ULocalPlayer* LocalPlayer = Widget.GetOwningLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UModularUIHoldSubsystem>() : nullptr;
}

void UModularUIHoldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// First, so it sees every press, even one something after it takes. It never takes any itself.
	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FModularUIHoldProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(Processor, 0);
	}
}

void UModularUIHoldSubsystem::Deinitialize()
{
	if (Processor)
	{
		Processor->Cancel();
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
		}
	}
	Processor.Reset();

	Super::Deinitialize();
}

void UModularUIHoldSubsystem::StartHold(const UWidget& Owner, const UInputAction& InputAction, FUIActionBindingHandle Binding, float HoldTime, const FSimpleDelegate& OnComplete)
{
	if (!Processor)
	{
		return;
	}

	// The keys the player has mapped to it now, so a rebound key holds it.
	const ULocalPlayer* LocalPlayer = Owner.GetOwningLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	TArray<FKey> Keys = EnhancedInput ? EnhancedInput->QueryKeysMappedToAction(&InputAction) : TArray<FKey>();
	Processor->Start(MoveTemp(Keys), Binding, HoldTime, OnComplete);
}
