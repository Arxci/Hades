#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidget.h"
#include "Containers/Ticker.h"
#include "Input/CommonUIInputTypes.h"
#include "Input/UIActionBindingHandle.h"

#include "ModularUIReveal.h"
#include "ModularUITransition.h"

#include "ModularActivatableWidget.generated.h"

class UInputAction;
class USoundBase;
class UTextBlock;
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
 * they end. Back, and Close Input Actions, close it the same way. Use Close Widget rather than Deactivate Widget, so the close plays.
 * Every key it binds is an Enhanced Input action players can rebind (see ModularUIInput.h). Accept stays Common UI's: Confirm Display Name
 * only says what it does here. On a widget that handles Back, the project's Extra Back Keys go Back too.
 * Transitions are for motion that depends on where and how the widget opened, or that many widgets share; widget animations are for
 * anything else, such as colors, materials and glows. Both play together, and a close waits for whichever ends last.
 * Description Text, the line along the bottom of a console menu, says what the player can do now: what the highlighted option of a menu
 * open over this one does, what to do in a mode the widget is in, such as "Select an item to combine with.", or what the highlighted
 * button does. It never says what a thing is: that belongs in a details panel. With nothing to say, it's empty.
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

	// Description
	/**
	 * Says Description in Description Text instead of what's highlighted, such as what to do in a mode the widget is in, "Select an item
	 * to combine with.", until Show Description is called again. Empty goes back to what's highlighted. Show Description From for this
	 * widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Description")
	void ShowDescription(const FText& Description);

	/**
	 * Says Description in Description Text on Source's behalf, such as a menu open over this widget saying what its highlighted option
	 * does, along the bottom of the screen as console menus say it. It stays until Source says something else, or Clear Description From.
	 * The source that started saying something last comes first, and as it stops, the one before it shows again, so a menu closing never
	 * clears what a newer one says. Empty is Clear Description From. Context menus do this themselves, with Show Description On Screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Description")
	void ShowDescriptionFrom(const UObject* Source, const FText& Description);

	/** Source stops saying anything in Description Text, such as a menu as it starts to close. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Description")
	void ClearDescriptionFrom(const UObject* Source);

	/** What Description Text says now. */
	UFUNCTION(BlueprintPure, Category = "Modular UI|Description")
	FText GetShownDescription() const { return ShownDescription; }

	/** Works out again what Description Text says, such as after what it describes changed. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Description")
	void RefreshDescription();

	/**
	 * A widget inside this one was highlighted: the mouse is over it, or focus is on it. Description Text says what it does from now on, by
	 * Get Description Of, until something else is highlighted, or with Keep Last Description off, until it isn't. Modular Buttons with a
	 * Description call it for themselves; call it for widgets of your own.
	 */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Description")
	void HighlightForDescription(UWidget* Widget);

	/** The widget isn't highlighted any more: the mouse left it, or focus did. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Description")
	void UnhighlightForDescription(UWidget* Widget);

	/**
	 * Describes what's highlighted with Description, for something highlighted that isn't a widget of its own, such as an entry a menu
	 * lists. Source tells it apart, for Clear Highlighted Description. Empty describes nothing, so Default Description shows: call it
	 * so for something that says nothing, such as an item under the cursor, so the last button's description isn't kept for it.
	 */
	void SetHighlightedDescription(const FText& Description, const UObject* Source);

	/**
	 * Source's highlight ended. Its description goes, unless Keep Last Description keeps it; with bEvenIfKept it goes either way, such as
	 * once what it describes is no longer on screen.
	 */
	void ClearHighlightedDescription(const UObject* Source, bool bEvenIfKept = false);

	/** Whether it has somewhere to show a description: Description Text or Description Panel. */
	bool HasDescriptionLine() const;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget Interface

	/** Before On Close Started, for C++ subclasses, such as to unbind actions that shouldn't work while it goes. */
	virtual void NativeOnCloseStarted() {}

	/**
	 * Binds Accept, only to show Confirm Display Name beside Accept's key in action bars: Accept itself still goes to the focused button.
	 * Override to bind it yourself, such as to click whatever the mouse is over.
	 */
	virtual void RegisterConfirmAction();

	/**
	 * Binds an input action, run while this is active, and shown in action bars by its Description with bDisplayInActionBar. With a Hold
	 * trigger on it, it runs once its key has been held for the trigger's Hold Time Threshold, and meanwhile the action bar shows the
	 * progress. Nothing is bound for a null action. Unregister the handle to unbind it.
	 */
	FUIActionBindingHandle RegisterInputAction(const UInputAction* InputAction, bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction);

	/**
	 * Adds parts of its own to the reveal, after Open Reveal's, such as a menu's panel that slides out of whatever it opened beside. Called
	 * once, as it's initialized.
	 */
	virtual void AddRevealParts(FModularUIReveal& InReveal) {}

	/** Where the reveal's transitions coming From Origin come from, such as away from what a menu opened beside. See the reveal's Set Origin. */
	void SetTransitionOrigin(const FVector2D& Away, const FVector2D& Corner);

	/**
	 * Calls Callback once Target is drawn, while this is still open and not closing, giving up after Timeout seconds. A new call replaces
	 * the last.
	 */
	void CallWhenDrawn(UWidget* Target, TFunction<void(UWidget&)>&& Callback, float Timeout = 1.0f);

	/** Close Widget was called: the close transition and animation have started, and it closes once they end. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Modular UI")
	void OnCloseStarted();

	/**
	 * What a highlighted widget does, for Description Text: a Modular Button's Description. Override it for widgets of your own, such as a
	 * menu's entries.
	 */
	virtual FText GetDescriptionOf(const UWidget& Widget) const;

	/**
	 * What the widget asks of the player now, such as "Select an area to place this item." while one is held, said in Description Text
	 * ahead of what's highlighted, though after what Show Description and menus over it say. Empty, the default, lets what's highlighted
	 * be described. Call Refresh Description as it changes.
	 */
	virtual FText GetHint() const { return FText::GetEmpty(); }

	/** What Description Text says changed, before On Description Changed, for C++ subclasses. */
	virtual void NativeOnDescriptionChanged(const FText& Description) {}

	/** What Description Text says changed, such as to animate it. After Description Text shows it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Modular UI|Description")
	void OnDescriptionChanged(const FText& Description);

	// Input
	/** Whether input reaches the game, the UI or both while this is active. Default leaves it to whatever else is active. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	EModularUIInputMode InputConfig = EModularUIInputMode::Default;

	/** How the mouse behaves while the game gets input. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (EditCondition = "InputConfig == EModularUIInputMode::Game || InputConfig == EModularUIInputMode::GameAndMenu"))
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

	/**
	 * What Accept does here, such as Select, shown in action bars beside Accept's key. Accept is Common UI's, set once in the project's Common
	 * UI Input Data, so there's no key to set here. Empty shows nothing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FText ConfirmDisplayName;

	/**
	 * Input actions that close this as Back does, such as the key that opened it, so the same key toggles it: I for an inventory, or M for a
	 * map. Not shown in action bars.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<const UInputAction>> CloseInputActions;

	/** Whether Confirm has anything to show: a Confirm Display Name, and an Accept in the project's Common UI Input Data. */
	bool HasConfirmAction() const;

	/** Binding arguments for Confirm: Common UI's Accept, named Confirm Display Name. */
	FBindUIActionArgs MakeConfirmBindArgs(bool bShouldDisplayInActionBar, const FSimpleDelegate& OnExecuteAction) const;

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

	// Description
	/**
	 * The line along the bottom of a console menu, saying what the player can do now: what a menu open over this one, or Show Description,
	 * says, the newest first, such as "Use the item." for a highlighted option; else what this widget asks, Get Hint, such as where to put
	 * a held item; else what the highlighted button does, such as "Adjust mouse and keyboard control settings."; else Default Description.
	 * Never what a thing is, such as an item's description: that goes in a details panel.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Description", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** Shown while there's a description, and hidden, not collapsed, while there isn't, so nothing around it moves. Put Description Text inside it. */
	UPROPERTY(BlueprintReadOnly, Category = "Description", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DescriptionPanel;

	/**
	 * What Description Text says while nothing else does, such as what the screen is for. Empty, as an inventory's line is while the
	 * player browses it, hides Description Panel then.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Description", meta = (MultiLine = true))
	FText DefaultDescription;

	/**
	 * Description Text keeps describing the last thing highlighted until something else is, as console menus do, so the mouse crossing a
	 * gap leaves it as it was. Off, it goes back to Default Description as soon as the mouse or focus leaves what it describes.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Description")
	bool bKeepLastDescription = true;

private:
	void SetPauseRequested(bool bRequested);

	/** Goes Back for one of the project's Extra Back Keys, while this is open and handles Back. False when it didn't. */
	bool HandleExtraBackKey(const FKey& Key);

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

	/** Something said in Description Text on a source's behalf, with Show Description From. */
	struct FSaidDescription
	{
		TWeakObjectPtr<const UObject> Source;
		FText Text;
	};

	// Description: what's said on sources' behalf, oldest first, what's highlighted and what highlighted it, and what Description Text says
	// now.
	TArray<FSaidDescription> SaidDescriptions;
	FText HighlightedDescription;
	TWeakObjectPtr<const UObject> HighlightedSource;
	FText ShownDescription;

	bool bClosing = false;
	bool bPauseRequested = false;
};
