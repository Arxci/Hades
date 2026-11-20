#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Containers/Ticker.h"

#include "ModularUIReveal.h"
#include "ModularUITransition.h"

#include "ModularActivatableWidget.generated.h"

class USoundBase;
class UWidgetAnimation;


/**
 * An activatable widget that comes in and goes with transitions, and can pause the game while it's open.
 * As it opens, Open Reveal brings its parts in one after another, such as a panel, then its title, then its text, and an animation named
 * OpenAnimation plays. Close Widget sends the parts back out and plays CloseAnimation, or OpenAnimation backwards, and deactivates it once
 * they end. Back closes it the same way. Use Close Widget rather than Deactivate Widget, so the close plays.
 */
UCLASS(Abstract, Blueprintable)
class MODULARUI_API UModularActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UModularActivatableWidget(const FObjectInitializer& ObjectInitializer);

	/** Closes it once its close transition and animation end. It takes no clicks or keys meanwhile. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI")
	void CloseWidget();

	/** Closes it at once, without its close transition or animation, or cuts a close short. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI")
	void CloseWidgetNow();

	/** Its close transition or animation is playing, and it closes once they end. */
	UFUNCTION(BlueprintPure, Category = "Modular UI")
	bool IsClosing() const { return bClosing; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget Interface

	/** Before On Close Started, for C++ subclasses, such as to unbind actions that shouldn't work while it goes. */
	virtual void NativeOnCloseStarted() {}

	/** Close Widget was called: the close transition and animation have started, and it closes once they end. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Modular UI")
	void OnCloseStarted();

	/** The world stops while this is open. It starts again as the close begins, so the world comes back while this goes. */
	UPROPERTY(EditDefaultsOnly, Category = "Pause")
	bool bPauseGame = false;

	/**
	 * The parts that come in as this opens, each once its own Delay has passed, such as a panel, then its title, then its text. Each names
	 * a widget in the designer. Leave their render opacity and render transform to the reveal.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (TitleProperty = "Widget"))
	TArray<FModularUIRevealStep> OpenReveal;

	/**
	 * Closing sends Open Reveal's parts back the way they came, all at once, and plays OpenAnimation backwards unless there's a
	 * CloseAnimation. Off, the parts all go with Close Transition.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	bool bCloseReversesOpen = true;

	/** How Open Reveal's parts all go as this closes, from however they look then. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (EditCondition = "!bCloseReversesOpen", EditConditionHides))
	FModularUITransition CloseTransition;

	/** Played as this opens, along with Open Reveal. Name an animation OpenAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> OpenAnimation;

	/** Played as this closes with Close Widget, along with its close transition. Name an animation CloseAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> CloseAnimation;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> OpenSound;

	/** Played as the close starts, however it closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> CloseSound;

private:
	void SetPauseRequested(bool bRequested);

	/**
	 * Moves the reveal on, and watches the close animation, from the core ticker, so both play with the game paused too. A close finishes
	 * once neither has anything left.
	 */
	void StartTicker();
	void FinishClosingWhenDone();

	FModularUIReveal Reveal;
	FTSTicker::FDelegateHandle TickHandle;

	/** The animation a close waits for. */
	TWeakObjectPtr<const UWidgetAnimation> AwaitedAnimation;

	bool bClosing = false;
	bool bPauseRequested = false;
};
