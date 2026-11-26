#include "ModularActivatableWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Input/CommonUIInputTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Editor/WidgetCompilerLog.h"
#endif

#include "ModularUIFocus.h"
#include "ModularUIPauseSubsystem.h"

#define LOCTEXT_NAMESPACE "ModularActivatableWidget"

UModularActivatableWidget::UModularActivatableWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Without Close Reverses Open, everything fades out together, where it is.
	CloseTransition.Duration = 0.2f;
	CloseTransition.bSlide = false;
}

TOptional<FUIInputConfig> UModularActivatableWidget::GetDesiredInputConfig() const
{
	switch (InputConfig)
	{
	case EModularUIInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case EModularUIInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case EModularUIInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EModularUIInputMode::Default:
		break;
	}
	return TOptional<FUIInputConfig>();
}

#if WITH_EDITOR
void UModularActivatableWidget::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);

	// Only menus need somewhere for gamepad focus to start. Subclasses in C++, such as context menus, usually find it themselves.
	if (InputConfig != EModularUIInputMode::Menu || GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UModularActivatableWidget, BP_GetDesiredFocusTarget)))
	{
		return;
	}

	const FText Message = LOCTEXT("NoFocusTarget", "Get Desired Focus Target isn't implemented, so gamepad focus has nowhere to start on this menu. If its C++ parent finds one, ignore this.");
	if (GetParentNativeClass(GetClass()) == UModularActivatableWidget::StaticClass())
	{
		CompileLog.Warning(Message);
	}
	else
	{
		CompileLog.Note(Message);
	}
}
#endif

void UModularActivatableWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The reveal adds to the render opacity and transform the parts have now, as set in the designer.
	Reveal.Init(*this, OpenReveal);
	AddRevealParts(Reveal);

	RegisterConfirmAction();
	if (CloseAction.IsValid())
	{
		RegisterUIActionBinding(FBindUIActionArgs(CloseAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::CloseWidget)));
	}
}

void UModularActivatableWidget::RegisterConfirmAction()
{
	if (!ConfirmAction.IsNull())
	{
		// Not consuming it lets Accept reach the focused button.
		FBindUIActionArgs ConfirmArgs(ConfirmAction, true, FSimpleDelegate::CreateWeakLambda(this, [] {}));
		ConfirmArgs.bConsumeInput = false;
		RegisterUIActionBinding(ConfirmArgs);
	}
}

void UModularActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	bClosing = false;
	AwaitedAnimation.Reset();
	AfterClosing = TFunction<void()>();
	SetPauseRequested(bPauseGame);
	if (OpenSound)
	{
		UGameplayStatics::PlaySound2D(this, OpenSound);
	}

	// From the start, as Common UI reuses widgets from however they last closed.
	Reveal.PlayIn();
	if (OpenAnimation)
	{
		PlayAnimation(OpenAnimation);
	}
	StartTicker();
}

void UModularActivatableWidget::NativeOnDeactivated()
{
	// The reveal stops where it got to: hidden after a close, and cut short by anything else. The next opening starts it afresh.
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle = FTSTicker::FDelegateHandle();
	ModularUI::CancelCallWhenDrawn(DrawnHandle);
	Reveal.Stop();
	if (const UWidgetAnimation* Awaited = AwaitedAnimation.Get())
	{
		StopAnimation(Awaited);
	}
	AwaitedAnimation.Reset();

	// Closed some other way while closing, such as at once: what was to happen after the close doesn't.
	const bool bWasClosing = bClosing;
	bClosing = false;
	AfterClosing = TFunction<void()>();
	SetPauseRequested(false);

	// A close that played made its sound as it started.
	if (!bWasClosing && CloseSound)
	{
		UGameplayStatics::PlaySound2D(this, CloseSound);
	}

	Super::NativeOnDeactivated();
}

bool UModularActivatableWidget::NativeOnHandleBackAction()
{
	if (!bIsBackHandler)
	{
		return Super::NativeOnHandleBackAction();
	}

	// Back closes it as the base class does, but once its close transition and animation end.
	if (!bClosing && !BP_OnHandleBackAction())
	{
		CloseWidget();
	}
	return true;
}

void UModularActivatableWidget::CloseWidget()
{
	CloseWidgetThen(TFunction<void()>());
}

void UModularActivatableWidget::CloseWidgetThen(TFunction<void()>&& AfterClose)
{
	if (bClosing || !IsActivated())
	{
		return;
	}

	// From here it takes no more input, and the world starts again, so it comes back while this goes.
	bClosing = true;
	AfterClosing = MoveTemp(AfterClose);
	ModularUI::CancelCallWhenDrawn(DrawnHandle);
	SetPauseRequested(false);
	if (CloseSound)
	{
		UGameplayStatics::PlaySound2D(this, CloseSound);
	}

	if (bCloseReversesOpen)
	{
		Reveal.ReverseIn();
	}
	else
	{
		Reveal.PlayOut(CloseTransition);
	}

	// Close Animation, or else Open Animation going back from wherever it got to.
	if (CloseAnimation)
	{
		AwaitedAnimation = CloseAnimation.Get();
		PlayAnimation(CloseAnimation);
	}
	else if (OpenAnimation && bCloseReversesOpen)
	{
		AwaitedAnimation = OpenAnimation.Get();
		if (IsAnimationPlaying(OpenAnimation))
		{
			ReverseAnimation(OpenAnimation);
		}
		else
		{
			PlayAnimationReverse(OpenAnimation);
		}
	}

	NativeOnCloseStarted();
	OnCloseStarted();

	// At once, when there's nothing to play.
	StartTicker();
	FinishClosingWhenDone();
}

void UModularActivatableWidget::CloseWidgetNow()
{
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

void UModularActivatableWidget::FocusWhenDrawn(UWidget* Target)
{
	CallWhenDrawn(Target, [](UWidget& Drawn)
	{
		Drawn.SetFocus();
	});
}

void UModularActivatableWidget::CallWhenDrawn(UWidget* Target, TFunction<void(UWidget&)>&& Callback)
{
	ModularUI::CallWhenDrawn(DrawnHandle, *this, Target, [this, Callback = MoveTemp(Callback)](UWidget& Drawn)
	{
		if (IsActivated() && !bClosing)
		{
			Callback(Drawn);
		}
	});
}

void UModularActivatableWidget::SetTransitionOrigin(const FVector2D& Away, const FVector2D& Corner)
{
	Reveal.SetOrigin(Away, Corner);
}

void UModularActivatableWidget::StartTicker()
{
	if (TickHandle.IsValid() || (!Reveal.IsPlaying() && !AwaitedAnimation.IsValid()))
	{
		return;
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
	{
		const bool bRevealing = Reveal.Advance(DeltaTime);
		const UWidgetAnimation* Awaited = AwaitedAnimation.Get();
		if (Awaited && !IsAnimationPlaying(Awaited))
		{
			AwaitedAnimation.Reset();
		}
		if (bRevealing || AwaitedAnimation.IsValid())
		{
			return true;
		}

		// Cleared before the close finishes, as finishing it can open this again, with a ticker of its own.
		TickHandle = FTSTicker::FDelegateHandle();
		FinishClosingWhenDone();
		return false;
	}));
}

void UModularActivatableWidget::FinishClosingWhenDone()
{
	if (bClosing && !Reveal.IsPlaying() && !AwaitedAnimation.IsValid())
	{
		FinishClosing();
	}
}

void UModularActivatableWidget::FinishClosing()
{
	// Deactivating forgets what was to happen after, so it's taken first.
	TFunction<void()> AfterClose = MoveTemp(AfterClosing);
	AfterClosing = TFunction<void()>();
	DeactivateWidget();
	if (AfterClose)
	{
		AfterClose();
	}
}

void UModularActivatableWidget::SetPauseRequested(bool bRequested)
{
	if (bRequested == bPauseRequested)
	{
		return;
	}

	bPauseRequested = bRequested;
	if (UModularUIPauseSubsystem* Pause = UModularUIPauseSubsystem::Get(this))
	{
		if (bRequested)
		{
			Pause->AddPauseRequest(this);
		}
		else
		{
			Pause->RemovePauseRequest(this);
		}
	}
}

FReply UModularActivatableWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// It sees a press before anything on it does, so while it's closing, nothing on it can be pressed.
	if (bClosing)
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UModularActivatableWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bClosing)
	{
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
