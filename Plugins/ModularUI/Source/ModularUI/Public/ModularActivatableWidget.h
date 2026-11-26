#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Containers/Ticker.h"
#include "Engine/DataTable.h"
#include "UITag.h"

#include "ModularUIReveal.h"
#include "ModularUITransition.h"

#include "ModularActivatableWidget.generated.h"

class USoundBase;
class UWidgetAnimation;
struct FUIInputConfig;


/** Whether input reaches the game, the UI or both while a widget is active. */
UENUM(BlueprintType)
enum class EModularUIInputMode : uint8
{
	/** Leaves the input mode to whatever else is active, such as the screen under this one. */
	Default,
	/** The game and the UI both get input, such as a HUD the player plays through, or a screen the player can keep walking with. */
	GameAndMenu,
	/** Only the game gets input. */
	Game,
	/** Only the UI gets input, and the cursor shows, such as a menu. */
	Menu
};


/**
 * The base for screens, menus and popups: an activatable widget that sets the input mode while it's active, comes in and goes with
 * transitions, and can pause the game while it's open.
 * As it opens, Open Reveal brings its parts in one after another, such as a panel, then its title, then its text, and an animation named
 * OpenAnimation plays. Close Widget sends the parts back out and plays CloseAnimation, or OpenAnimation backwards, and deactivates it once
 * they end. Back, and Close Action, close it the same way. Use Close Widget rather than Deactivate Widget, so the close plays.
 * Transitions are for motion that depends on where and how the widget opened, or that many widgets share; widget animations are for
 * anything else, such as colors, materials and glows. Both play together, and a close waits for whichever ends last.
 */
UCLASS(Abstract, Blueprintable)
class MODULARUI_API UModularActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UModularActivatableWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommonActivatableWidget Interface
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	//~ End UCommonActivatableWidget Interface

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

	/** Closes it once its close transition and animation end. It takes no clicks or keys meanwhile. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI")
	void CloseWidget();

	/** Close Widget, then After Close once it has closed. Nothing runs if it's closed some other way first, such as with Close Widget Now. */
	void CloseWidgetThen(TFunction<void()>&& AfterClose);

	/** Closes it at once, without its close transition or animation, or cuts a close short. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI")
	void CloseWidgetNow();

	/** Its close transition or animation is playing, and it closes once they end. */
	UFUNCTION(BlueprintPure, Category = "Modular UI")
	bool IsClosing() const { return bClosing; }

	/**
	 * Gives Target focus once it's drawn and can take it, such as a cell on a page a tab switcher is still bringing in, or a button a menu
	 * has only just shown. Gives up after a second, and when this closes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Focus")
	void FocusWhenDrawn(UWidget* Target);

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

	/**
	 * Registers Confirm Action, only to show what Accept does in an action bar: Accept itself still goes to the focused button. Override to
	 * bind it yourself, such as to click whatever the mouse is over.
	 */
	virtual void RegisterConfirmAction();

	/**
	 * Adds parts of its own to the reveal, after Open Reveal's, such as a menu's panel that slides out of whatever it opened beside. Called
	 * once, as it's initialized.
	 */
	virtual void AddRevealParts(FModularUIReveal& InReveal) {}

	/** Where the reveal's transitions coming From Origin come from, such as away from what a menu opened beside. See the reveal's Set Origin. */
	void SetTransitionOrigin(const FVector2D& Away, const FVector2D& Corner);

	/** Calls Callback once Target is drawn, while this is still open and not closing. A new call replaces the last. */
	void CallWhenDrawn(UWidget* Target, TFunction<void(UWidget&)>&& Callback);

	/** Close Widget was called: the close transition and animation have started, and it closes once they end. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Modular UI")
	void OnCloseStarted();

	// Input
	/** Whether input reaches the game, the UI or both while this is active. Default leaves it to whatever else is active. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	EModularUIInputMode InputConfig = EModularUIInputMode::Default;

	/** How the mouse behaves while the game gets input. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (EditCondition = "InputConfig == EModularUIInputMode::Game || InputConfig == EModularUIInputMode::GameAndMenu"))
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	/** Shown in an action bar for what Accept does here, such as Select. A row of a UI input action table. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ConfirmAction;

	/**
	 * A UI action that closes this as Back does, such as the key that opened it, so the same key toggles it: Tab for an inventory, or M
	 * for a map. Not shown in action bars.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (Categories = "UI.Action"))
	FUIActionTag CloseAction;

	// Pause
	/** The world stops while this is open. It starts again as the close begins, so the world comes back while this goes. */
	UPROPERTY(EditDefaultsOnly, Category = "Pause")
	bool bPauseGame = false;

	// Transition
	/**
	 * The parts that come in as this opens, each once its own Delay has passed, such as a panel, then its title, then its text. Each names
	 * a widget in the designer. Leave their render opacity and render transform to the reveal.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (TitleProperty = "Widget"))
	TArray<FModularUIRevealStep> OpenReveal;

	/**
	 * Closing sends the parts back the way they came, all at once, from wherever they got to, and plays OpenAnimation backwards unless
	 * there's a CloseAnimation. Off, the parts all go with Close Transition.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	bool bCloseReversesOpen = true;

	/** How the parts all go as this closes, from however they look then, such as a quick fade. */
	UPROPERTY(EditDefaultsOnly, Category = "Transition", meta = (EditCondition = "!bCloseReversesOpen", EditConditionHides))
	FModularUITransition CloseTransition;

	/** Played as this opens, along with the reveal. Name an animation OpenAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> OpenAnimation;

	/** Played as this closes with Close Widget, along with its close transition. Name an animation CloseAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> CloseAnimation;

	// Sound
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
	void FinishClosing();

	FModularUIReveal Reveal;
	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle DrawnHandle;

	/** The animation a close waits for. */
	TWeakObjectPtr<const UWidgetAnimation> AwaitedAnimation;

	/** What happens once a close started with Close Widget Then has finished. */
	TFunction<void()> AfterClosing;

	bool bClosing = false;
	bool bPauseRequested = false;
};
