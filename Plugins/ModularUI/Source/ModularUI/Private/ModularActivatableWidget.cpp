#include "ModularActivatableWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "ModularUIPauseSubsystem.h"

UModularActivatableWidget::UModularActivatableWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Without Close Reverses Open, everything fades out together, where it is.
	CloseTransition.Duration = 0.2f;
	CloseTransition.bSlide = false;
}

void UModularActivatableWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The reveal adds to the render opacity and transform the parts have now, as set in the designer.
	Reveal.Init(*this, OpenReveal);
}

void UModularActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	bClosing = false;
	AwaitedAnimation.Reset();
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
	Reveal.Stop();
	if (const UWidgetAnimation* Awaited = AwaitedAnimation.Get())
	{
		StopAnimation(Awaited);
	}
	AwaitedAnimation.Reset();

	const bool bWasClosing = bClosing;
	bClosing = false;
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
	if (bClosing || !IsActivated())
	{
		return;
	}

	// From here it takes no more input, and the world starts again, so it comes back while this goes.
	bClosing = true;
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
		DeactivateWidget();
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
