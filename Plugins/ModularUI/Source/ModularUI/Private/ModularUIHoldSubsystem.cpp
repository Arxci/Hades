#include "ModularUIHoldSubsystem.h"

#include "Components/Widget.h"
#include "Containers/Ticker.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/PlatformTime.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionBinding.h"
#include "InputAction.h"

#include "ModularUIHoldTimer.h"

/**
 * Watches one local player's keys, without taking any input, and times their hold in progress from the core ticker, which runs with the game
 * paused too. Knowing when a key went down from before the hold starts, it times a hold from the press itself, and knowing when the last key
 * came up, a key let go just short of the time never completes it. Each frame of the hold, it checks the hold's binding still takes input.
 */
class FModularUIHoldProcessor final : public IInputProcessor, public TSharedFromThis<FModularUIHoldProcessor>
{
public:
	explicit FModularUIHoldProcessor(ULocalPlayer& InPlayer) : Player(&InPlayer)
	{
	}

	virtual ~FModularUIHoldProcessor() override
	{
		StopTicker();
	}

	void Start(const TArray<FKey>& Keys, FUIActionBindingHandle InBinding, float HoldSeconds, const FSimpleDelegate& InOnComplete)
	{
		const bool bWasHolding = Timer.IsHolding();
		const FUIActionBindingHandle Replaced = Binding;
		if (!Timer.Start(Keys, HoldSeconds))
		{
			return;
		}

		// The hold of another press it replaced ends without its action, and its progress drops to 0.
		if (bWasHolding)
		{
			ShowProgress(Replaced, 0.0f);
		}
		Binding = InBinding;
		OnComplete = InOnComplete;
		StartTicker();
	}

	/** Ends the hold in progress without running it. */
	void Cancel()
	{
		if (Timer.IsHolding())
		{
			Timer.Cancel();
			Finish(/*bCompleted*/ false);
		}
	}

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		// Keys let go while the game is in the background never say so.
		if (!SlateApp.IsActive() && Timer.HasKeysDown())
		{
			Timer.ReleaseAll(FPlatformTime::Seconds());
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (!InKeyEvent.IsRepeat() && IsPlayersInput(InKeyEvent.GetUserIndex()))
		{
			Timer.Press(InKeyEvent.GetKey(), FPlatformTime::Seconds());
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (IsPlayersInput(InKeyEvent.GetUserIndex()))
		{
			Timer.Release(InKeyEvent.GetKey(), FPlatformTime::Seconds());
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (IsPlayersInput(MouseEvent.GetUserIndex()))
		{
			Timer.Press(MouseEvent.GetEffectingButton(), FPlatformTime::Seconds());
		}
		return false;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (IsPlayersInput(MouseEvent.GetUserIndex()))
		{
			Timer.Release(MouseEvent.GetEffectingButton(), FPlatformTime::Seconds());
		}
		return false;
	}

	/** The second of two quick presses of a mouse button comes as a double-click: a press like the first. */
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		return HandleMouseButtonDownEvent(SlateApp, MouseEvent);
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("ModularUIHold"); }
	//~ End IInputProcessor Interface

private:
	/** Whether input came from the player's own devices, as Slate tells players apart, so another player's press never holds this one's. */
	bool IsPlayersInput(uint32 UserIndex) const
	{
		ULocalPlayer* LocalPlayer = Player.Get();
		const TSharedPtr<FSlateUser> SlateUser = LocalPlayer ? LocalPlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
		return SlateUser && static_cast<uint32>(SlateUser->GetUserIndex()) == UserIndex;
	}

	/** Whether the hold's binding still takes input: it's one of the player's active bindings, such as those action bars show. */
	bool IsBindingActive() const
	{
		ULocalPlayer* LocalPlayer = Player.Get();
		UCommonUIActionRouterBase* Router = LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
		return Router && Binding.IsValid() && Router->GatherActiveBindings().Contains(Binding);
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
		// The action a hold runs can take the player's subsystem away, such as by removing the player, so this lives until it returns.
		const TSharedRef<FModularUIHoldProcessor> KeepAlive = AsShared();

		// Its binding stopped taking input, such as under a menu that opened over its widget, or as its widget closed or unbound it: the hold
		// goes without its action.
		if (Timer.IsHolding() && !IsBindingActive())
		{
			Timer.Cancel();
			Finish(/*bCompleted*/ false);
		}

		float Progress = 0.0f;
		switch (Timer.Advance(FPlatformTime::Seconds(), Progress))
		{
		case FModularUIHoldTimer::EStep::Holding:
			ShowProgress(Binding, Progress);
			break;
		case FModularUIHoldTimer::EStep::Completed:
			Finish(/*bCompleted*/ true);
			break;
		case FModularUIHoldTimer::EStep::Cancelled:
			Finish(/*bCompleted*/ false);
			break;
		case FModularUIHoldTimer::EStep::None:
			break;
		}

		// The action that just ran can start another hold, which this ticker goes on to time.
		if (Timer.IsHolding())
		{
			return true;
		}
		TickerHandle = FTSTicker::FDelegateHandle();
		return false;
	}

	static void ShowProgress(const FUIActionBindingHandle& ShownBinding, float Progress)
	{
		if (const TSharedPtr<FUIActionBinding> FoundBinding = FUIActionBinding::FindBinding(ShownBinding))
		{
			FoundBinding->OnHoldActionProgressed.Broadcast(Progress);
		}
	}

	/**
	 * Ends the hold the timer just ended. Completed, it reports 1 first, so a button can show the hold finished. Either way the progress drops
	 * to 0 at once, and the hold is over before the action runs, so the action can start another.
	 */
	void Finish(bool bCompleted)
	{
		const FUIActionBindingHandle Ended = Binding;
		const FSimpleDelegate Completed = OnComplete;
		Binding = FUIActionBindingHandle();
		OnComplete.Unbind();

		if (bCompleted)
		{
			ShowProgress(Ended, 1.0f);
		}
		ShowProgress(Ended, 0.0f);
		if (bCompleted)
		{
			Completed.ExecuteIfBound();
		}
	}

	TWeakObjectPtr<ULocalPlayer> Player;
	FModularUIHoldTimer Timer;

	/** The hold in progress: its binding, and what runs once it completes. */
	FUIActionBindingHandle Binding;
	FSimpleDelegate OnComplete;

	FTSTicker::FDelegateHandle TickerHandle;
};

UModularUIHoldSubsystem* UModularUIHoldSubsystem::Get(const UWidget& Widget)
{
	return ULocalPlayer::GetSubsystem<UModularUIHoldSubsystem>(Widget.GetOwningLocalPlayer());
}

void UModularUIHoldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// First at the time it's registered, so it sees presses before whatever comes after it. It never takes any itself.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer && FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FModularUIHoldProcessor>(*LocalPlayer);
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

void UModularUIHoldSubsystem::StartHold(const UInputAction& InputAction, FUIActionBindingHandle ActionBinding, float HoldSeconds, const FSimpleDelegate& OnComplete)
{
	if (!Processor)
	{
		return;
	}

	// The keys the player has mapped to it now, so a rebound key holds it.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const TArray<FKey> Keys = EnhancedInput ? EnhancedInput->QueryKeysMappedToAction(&InputAction) : TArray<FKey>();
	Processor->Start(Keys, ActionBinding, HoldSeconds, OnComplete);
}
