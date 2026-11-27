#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"

#include "ModularActivatableWidget.h"
#include "Widgets/ModularPromptTypes.h"

#include "ModularContextMenuWidget.generated.h"

class UCommonButtonBase;
class UModularPromptWidget;
class UPanelWidget;
class USlider;
class UTextBlock;
class UTexture2D;


/** Where a context menu opens. Each but As Designed then moves in as far as it takes to stay on screen. */
UENUM(BlueprintType)
enum class EModularMenuPlacement : uint8
{
	/**
	 * Beside its anchor, on the side toward the middle of the screen: level with the anchor's top in the upper half, and with its bottom in
	 * the lower half.
	 */
	TowardCenter,
	/** On its anchor's right, level with its top. It goes to the left where only that side has room. */
	Right,
	/** On its anchor's left, level with its top. It goes to the right where only that side has room. */
	Left,
	/**
	 * Wherever Menu Panel is in the designer, such as the middle of the screen for a modal picker over a dim backdrop. The anchor is only
	 * what it opened for.
	 */
	AsDesigned,
	/**
	 * At the cursor, down and to the right of it as desktop context menus open, or up or to the left where only that side has room. Without
	 * the mouse, such as on a gamepad, it opens as Toward Center does.
	 */
	AtCursor
};


/** What a click off a context menu does, with any mouse button but the right, whose clicks Right Click decides. */
UENUM(BlueprintType)
enum class EModularMenuOutsideClick : uint8
{
	/** Closes the menu, and the click goes no further, so a stray click only closes it. */
	Close,
	/**
	 * Closes the menu and goes on to whatever is under it, as if the menu weren't open, so the player carries on at once, such as dragging
	 * another item. Only where the menu knows how to reach what's under it, such as the items under an item menu; elsewhere it only closes
	 * the menu.
	 */
	PassThrough,
	/** Nothing: the menu stays open until the player picks an entry or goes Back, as in a menu that must be answered. */
	Block
};


/** What a right-click does while a context menu is open, on it or off it. */
UENUM(BlueprintType)
enum class EModularMenuRightClick : uint8
{
	/**
	 * Back, wherever the cursor is: an open prompt is answered with no, or else the menu closes, a step at a time as Back goes. For games
	 * where the right mouse button means Back, as the gamepad's Back button does on consoles.
	 */
	Back,
	/**
	 * As desktop context menus do: a right-click off the menu closes it and goes on to whatever is under it, as Outside Click's Pass Through
	 * does, so right-clicking another item opens that one's menu in its place. A right-click on the menu does nothing.
	 */
	PassThrough,
	/** Nothing, on the menu or off it. */
	Ignore
};


/** Where a context menu asks its question when an entry needs an answer first, such as to confirm or how many. */
UENUM(BlueprintType)
enum class EModularPromptPresentation : uint8
{
	/**
	 * In Prompt Panel, in place of the entries: the menu becomes the question and goes back to its entries after. As compact as it gets,
	 * and the player's eyes stay where they were. Lay Prompt Panel out inside Menu Panel.
	 */
	Inline,
	/**
	 * In Prompt Panel wherever it's laid out, such as the middle of the screen, over the menu, which stays where it is with its entries
	 * disabled. Prompt Backdrop, such as a dim full-screen image, shows behind the question.
	 */
	Overlay,
	/** In a Prompt Class dialog of its own, pushed onto Prompt Layer, so one dialog serves every menu, as a game's confirmation dialog. */
	Dialog
};


/** An entry a context menu makes for itself, such as from Blueprint for a menu built as it opens. */
USTRUCT(BlueprintType)
struct MODULARUI_API FModularMenuEntry
{
	GENERATED_BODY()

	/** What On Entry Chosen tells the entry apart by. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	FName Id;

	/** The entry's label, on entry buttons that are Modular Buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	FText Label;

	/** What it does, shown in Description Text while it's highlighted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (MultiLine = true))
	FText Description;

	/** On entry buttons that are Modular Buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	TObjectPtr<UTexture2D> Icon;

	/** Off, it's listed disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	bool bEnabled = true;

	/** Asks Prompt first, and the entry is only chosen with a yes, with the count chosen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	bool bAskFirst = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (EditCondition = "bAskFirst"))
	FModularPromptRequest Prompt;

	/** The menu stays open once it's chosen. Off, choosing it closes the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	bool bKeepsMenuOpen = false;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularMenuEntryChosenSignature, FName, EntryId, int32, Count);


/**
 * A small menu opened beside something, such as Use, Equip and Drop beside an item, or Set Waypoint beside a map marker: push it onto a
 * layer, give it an anchor, and it places itself beside it, staying on screen. One entry is highlighted at a time: the first as it opens,
 * then whichever the mouse or focus reached last. What it does, from Get Entry Description, shows in the menu's Description Text, and with
 * Show Description On Screen, along the bottom of the screen it opened from, as console menus say it.
 * Entries are the buttons in Menu Panel: placed in the designer, whose On Clicked you bind, or made with Add Entry, which On Entry Chosen
 * reports. An entry can ask first, to confirm or how many, inline in place of the entries, over the menu, or in a dialog of its own.
 * Back closes it, as do the presses off it that Outside Click and Right Click say. It comes in with Open Transition, out of whatever it
 * opened beside, and plays OpenAnimation and CloseAnimation too.
 * Give the root a full-screen, hit-testable border, which can be a dim backdrop for a modal menu, so presses off the menu reach it and
 * nothing behind it can be hovered while it's open. Put Menu Panel in a Canvas Panel filling the screen, with Size To Content ticked.
 */
UCLASS(Abstract, Blueprintable)
class MODULARUI_API UModularContextMenuWidget : public UModularActivatableWidget
{
	GENERATED_BODY()

public:
	UModularContextMenuWidget(const FObjectInitializer& ObjectInitializer);

	// Anchor
	/**
	 * Opens the menu beside Anchor Widget, as Placement says. Call it as the menu is pushed, before it shows, or later to move it. Without
	 * an anchor, it opens beside the focused widget on a gamepad, or at the cursor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Anchor")
	void SetAnchorWidget(UWidget* InAnchorWidget);

	/** Opens the menu beside a box on the screen, in absolute (desktop) space, such as a mouse event's Screen Space Position and no size. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Anchor")
	void SetAnchorBox(FVector2D AbsolutePosition, FVector2D AbsoluteSize);

	/** Opens the menu beside the widget focused now on a gamepad, or else at the cursor. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Anchor")
	void SetAnchorToFocusOrCursor();

	/** The widget the menu opened beside, if it was given one. */
	UFUNCTION(BlueprintPure, Category = "Context Menu|Anchor")
	UWidget* GetAnchorWidget() const { return AnchorWidget.Get(); }

	/**
	 * The screen whose Description Text says what the highlighted entry does, with Show Description On Screen. Without one, it's the screen
	 * the menu opened from: the nearest Modular Activatable Widget with a description line around its Anchor Widget. Call it as the menu is
	 * pushed, for a menu anchored to a box or the cursor. Forgotten as the menu closes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Description")
	void SetDescriptionScreen(UModularActivatableWidget* Screen);

	// Entries
	/**
	 * Makes an Entry Button Class button in Entry Panel for the entry, after any there already, and returns it. On Entry Chosen reports it
	 * once picked. Its label and icon show on buttons that are Modular Buttons.
	 */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Entries")
	UCommonButtonBase* AddEntry(const FModularMenuEntry& Entry);

	/** Removes every entry Add Entry made. Buttons placed in the designer stay. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Entries")
	void ClearEntries();

	/** The entry highlighted now: the first as the menu opens, then whichever the mouse or focus reached last. The prompt's buttons included. */
	UFUNCTION(BlueprintPure, Category = "Context Menu|Entries")
	UCommonButtonBase* GetHighlightedEntry() const { return HighlightedEntry.Get(); }

	/** Highlights an entry, and with Focus, gives it focus, so Accept picks it. Anything but an entry leaves the highlight where it was. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Entries")
	void SetHighlightedEntry(UCommonButtonBase* Entry, bool bFocus = true);

	// Closing
	/** Closes the menu after its close transition and Close Animation. Use it instead of Deactivate Widget so they play. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu")
	void CloseMenu();

	/** Closes the menu at once, or cuts a close short, such as when another menu replaces it. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu")
	void CloseMenuNow();

	// Prompt
	/**
	 * Asks Request, as Prompt Presentation says, then calls On Answered once with the answer: yes with the count chosen, or no. Without
	 * anywhere to ask, it answers yes at once, with the count the prompt would start at.
	 */
	void ShowPrompt(const FModularPromptRequest& Request, TFunction<void(bool bConfirmed, int32 Count)>&& OnAnswered);

	/** Asks Request, as Prompt Presentation says. On Prompt Answered says the answer. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Prompt", meta = (DisplayName = "Show Prompt"))
	void K2_ShowPrompt(const FModularPromptRequest& Request);

	/** A prompt is waiting for an answer. */
	UFUNCTION(BlueprintPure, Category = "Context Menu|Prompt")
	bool IsPrompting() const { return bPrompting; }

	/** Answers the open prompt with no, and goes back to the entries. */
	UFUNCTION(BlueprintCallable, Category = "Context Menu|Prompt")
	void CancelPrompt();

	/** An entry Add Entry made was picked, after its prompt was answered yes, with the count chosen, or 0. */
	UPROPERTY(BlueprintAssignable, Category = "Context Menu|Entries")
	FModularMenuEntryChosenSignature OnEntryChosen;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget Interface

	//~ Begin UModularActivatableWidget Interface
	virtual void NativeOnCloseStarted() override;
	virtual void RegisterConfirmAction() override;
	virtual void AddRevealParts(FModularUIReveal& InReveal) override;
	virtual FText GetDescriptionOf(const UWidget& Widget) const override;
	//~ End UModularActivatableWidget Interface

	/**
	 * The screen whose Description Text says what the highlighted entry does, with Show Description On Screen: the one Set Description
	 * Screen gave, or else the one the menu opened from, around its anchor. Override it to find another, such as the screen an item menu
	 * is for. Null says it only in the menu's own Description Text.
	 */
	virtual UModularActivatableWidget* FindDescriptionScreen() const;

	/**
	 * A press off the menu that should pass through, as Outside Click or Right Click say: reach whatever is under it and return true with
	 * the reply. Close the menu with Close Menu Now as the press reaches it, at once, or once it's known what the press becomes, such as a
	 * click or a drag. False, the default, closes the menu as Close does.
	 */
	virtual bool PassPressThrough(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, FReply& OutReply);

	/** Whether a point in absolute (desktop) space is on the menu: on Menu Panel, or on Prompt Panel while a prompt shows in it. */
	bool IsOnMenu(const FVector2D& ScreenPosition) const;

	/** What an entry does, for Description Text while it's highlighted. An Add Entry entry's Description by default. */
	virtual FText GetEntryDescription(UCommonButtonBase* Entry) const;

	/** The highlighted entry changed, before On Entry Highlighted, for C++ subclasses. */
	virtual void NativeOnEntryHighlighted(UCommonButtonBase* Entry, const FText& Description) {}

	/** The widget holding the entries: collapsed while an inline prompt asks, and disabled while an overlay prompt does. Entry Panel by default. */
	virtual UWidget* GetEntriesWidget() const;

	/** Where the menu opens beside, in absolute (desktop) space: the anchor's box, or the box it was given. */
	FBox2D GetAnchorBox() const { return AnchorBox; }

	/**
	 * Places the menu beside its anchor, measured with the entries it shows now, such as after they changed. Again on the next frame, as a
	 * menu placed before it's first drawn can't know its canvas yet.
	 */
	void PlaceMenu();

	/** Gives Get Desired Focus Target focus once it's drawn, such as once entries that just showed are laid out. */
	void FocusDesiredTarget();

	/** Highlights an entry, and with bFocus, focuses it too. Anything but an entry leaves the highlight where it was. */
	void HighlightEntry(UCommonButtonBase* Entry, bool bFocus);
	void ClearHighlight();

	/**
	 * Says Description for the highlighted entry, such as why it can't be picked, until another entry is highlighted: in Description Text,
	 * and with Show Description On Screen, on the screen's too.
	 */
	void DescribeHighlightedEntry(const FText& Description);

	/** Whether the button is an entry: one of Menu Panel's buttons, or the prompt's. */
	bool IsEntry(UCommonButtonBase* Button) const;

	/** Whether the button is one of the prompt's, such as Confirm Button. */
	bool IsPromptButton(UCommonButtonBase* Button) const;

	/**
	 * Has Slate work out again what's under the cursor on the next frame, so a button hidden or moved out from under it stops looking
	 * hovered, such as the entry whose prompt replaced the entries.
	 */
	void RefreshHover();

	/** Where the cursor is, in absolute (desktop) space, while the mouse is in use. Unset on a gamepad, where the cursor is hidden. */
	TOptional<FVector2D> GetCursorPosition() const;

	/** The highlighted entry changed. Show anything else about it here. Description is what Description Text says. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Context Menu")
	void OnEntryHighlighted(UCommonButtonBase* Entry, const FText& Description);

	/** The prompt Show Prompt asked was answered: yes with the count chosen, or no. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Context Menu|Prompt")
	void OnPromptAnswered(bool bConfirmed, int32 Count);

	// Layout
	/**
	 * The menu itself. Put it in a Canvas Panel that fills the screen, with Size To Content ticked, so it can be moved beside its anchor. As
	 * the menu opens, its first visible button is highlighted and takes focus, unless Get Desired Focus Target is implemented.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu", meta = (BindWidget))
	TObjectPtr<UWidget> MenuPanel;

	/**
	 * Everything the menu draws, its background and its entries, as one widget inside Menu Panel, such as a Border holding them. Only needed
	 * for Slide From Behind Anchor: Menu Panel stays where the menu ends up and clips it, while it slides out from behind the anchor, so give
	 * Menu Panel no background or padding of its own.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuContent;

	/** Where the menu opens. As Designed keeps it where it's laid out, such as centered for a modal picker. */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Placement")
	EModularMenuPlacement Placement = EModularMenuPlacement::TowardCenter;

	/** Space between the anchor, or the cursor, and the menu, in pixels. */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Placement", meta = (ClampMin = "0", EditCondition = "Placement != EModularMenuPlacement::AsDesigned"))
	float MenuGap = 0.0f;

	/** Space kept between the menu and each edge of the screen, in pixels. */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Placement", meta = (EditCondition = "Placement != EModularMenuPlacement::AsDesigned"))
	FMargin ScreenPadding;

	// Dismissing
	/**
	 * What a click off the menu does, with any mouse button but the right: Close only closes it, Pass Through closes it and goes on to what's
	 * under it, and Block keeps it open until an entry is picked or Back.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Dismissing")
	EModularMenuOutsideClick OutsideClick = EModularMenuOutsideClick::Close;

	/**
	 * What a right-click does, on the menu or off it: Back goes back a step, wherever the cursor is, as in games whose right mouse button means
	 * Back; Pass Through does as desktop menus do, closing it and reaching what's under a right-click off it; Ignore does nothing.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Dismissing")
	EModularMenuRightClick RightClick = EModularMenuRightClick::PassThrough;

	// Highlight
	/**
	 * One entry is always highlighted, as console-style menus keep one: the first as the menu opens, or as a prompt asks, then whichever
	 * the mouse or focus reached last, so Accept picks the one the player sees. It's shown with its button style's Selected look: give
	 * Selected Base and Selected Hovered your highlight, and Selected Text Style its text, the prompt's buttons too. On a gamepad, focus alone
	 * moves it. Off, an entry looks highlighted only while it's hovered, or focused on a gamepad, though the first still takes focus.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Highlight")
	bool bKeepEntryHighlighted = false;

	// Description
	/**
	 * What the highlighted entry does, such as "Use the item.", shows in the Description Text of the screen the menu opened from too, along
	 * the bottom of the screen as console menus say it, until the menu starts to close. The menu's own Description Text, if it has one,
	 * says it either way. Off, only the menu's own says it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Description")
	bool bShowDescriptionOnScreen = true;

	// Entries
	/** Holds the entries Add Entry makes, after any placed in it, such as a Vertical Box. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Entries", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EntryPanel;

	/** What Add Entry makes. A Modular Button shows the entry's label and icon. */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Entries")
	TSubclassOf<UCommonButtonBase> EntryButtonClass;

	// Prompt
	/**
	 * Where an entry's question goes: Inline in place of the entries, Overlay over the menu wherever Prompt Panel is laid out, or a Dialog of
	 * its own on Prompt Layer.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Prompt")
	EModularPromptPresentation PromptPresentation = EModularPromptPresentation::Inline;

	/**
	 * Shown while a prompt asks, Inline or Overlay. Put Prompt Text, Confirm Button and Cancel Button inside it, and for counts, Quantity
	 * Slider and Quantity Text. Back, or Cancel Button, goes back to the entries.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptPanel;

	/** Shown behind an Overlay prompt, such as a dim full-screen image above Menu Panel, so the menu can't be pressed meanwhile. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptBackdrop;

	/** The request's Title, such as the item the question is about. Collapsed while it's empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptTitleText;

	/** The question. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	/** Answers yes. Needed for Inline and Overlay prompts. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ConfirmButton;

	/** Answers no, and goes back to the entries, highlighting the one that asked. A confirmation can start on it, as Confirm Starts On Cancel says. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CancelButton;

	/** Chooses how many, for prompts that ask. Gamepad focus starts on it, so left and right change the count. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<USlider> QuantitySlider;

	/** The count chosen, in Quantity Format. */
	UPROPERTY(BlueprintReadOnly, Category = "Context Menu|Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	/** What Quantity Text says. {Count} is the count chosen, and {Max} the most there is to choose. */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Prompt")
	FText QuantityFormat = INVTEXT("{Count}");

	/**
	 * A confirmation starts highlighted on Cancel Button, so the quick answer is the safe one, such as before discarding. Off, it starts on
	 * the prompt's first button, as the menu starts on its first entry. A count to choose starts on Quantity Slider either way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Prompt")
	bool bConfirmStartsOnCancel = true;

	/** The dialog a Dialog prompt asks in. */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Prompt", meta = (EditCondition = "PromptPresentation == EModularPromptPresentation::Dialog"))
	TSubclassOf<UModularPromptWidget> PromptClass;

	/**
	 * The layer a Dialog prompt is pushed onto, a Modular UI layer. Pick one above the menu's own, or the menu would be put aside under the
	 * dialog, as a stack only shows its top. Without a layer it asks Inline instead.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Context Menu|Prompt", meta = (Categories = "UI.Layer", EditCondition = "PromptPresentation == EModularPromptPresentation::Dialog"))
	FGameplayTag PromptLayer;

	// Transition
	/**
	 * How the menu comes in as it opens: a fade, a slide and a growth, each optional. The whole menu fades, and Menu Panel slides and grows
	 * from wherever it opened, such as out from its anchor. It adds to the render opacity and transform they have in the designer. Leave
	 * the layer's own transition at 0, so the two don't play over each other.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	FModularUITransition OpenTransition;

	/**
	 * The menu comes out from behind what it opened beside, rather than over it, as a drawer pulled out from under it: Menu Panel clips Menu
	 * Content, which slides out of Menu Panel's edge nearest the anchor, from all the way behind it, so nothing of the menu ever covers the
	 * anchor. Open Transition's timing, easing, fade and growth still apply, and its slide covers the menu's whole width or height, whatever
	 * its Slide Distance. Needs Menu Content inside Menu Panel; without it, the menu slides as Open Transition says.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Transition")
	bool bSlideFromBehindAnchor = false;

	/** Whether the menu slides from behind its anchor: Slide From Behind Anchor is on, with Menu Content inside Menu Panel. */
	bool SlidesFromBehindAnchor() const;

private:
	/** An entry Add Entry made, and its button. */
	struct FMadeEntry
	{
		TWeakObjectPtr<UCommonButtonBase> Button;
		FModularMenuEntry Entry;
	};

	void PlaceBeside(const FBox2D& Box);
	void PlaceNextFrame();
	bool GetCanvasGeometry(FGeometry& OutGeometry) const;
	FVector2D AbsoluteToCanvas(const FVector2D& AbsolutePoint);
	FVector2D GetCanvasSize();
	FVector2D MeasureMenuPanel();

	/** Back, from a key or a right-click: answers a prompt with no, or closes the menu. */
	void GoBack();

	/** A press off the menu: passes it through where it can, or closes the menu. */
	FReply PressOutside(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, bool bPassThrough);

	void ForEachEntry(TFunctionRef<void(UCommonButtonBase&)> Callback) const;
	UCommonButtonBase* FindHoveredButton() const;

	void HandleMadeEntryClicked(int32 Index);
	void ChooseEntry(FName EntryId, int32 Count, bool bKeepsMenuOpen);

	// Prompt
	void ShowPromptHere(const FModularPromptRequest& Request, EModularPromptPresentation Presentation);
	void HidePrompt();

	/** Highlights the entry that asked again, or where the menu starts if the entries changed under the prompt. */
	void RestoreHighlightAfterPrompt();
	void AnswerPrompt(bool bConfirmed);
	void HandleDialogAnswered(bool bConfirmed, int32 Count);
	void HandlePromptConfirmed();
	void HandlePromptCancelled();

	UFUNCTION()
	void HandleQuantityChanged(float Value);

	/** Confirm's key was pressed: it only shows what Accept does, so Accept goes on, to the entry the player sees, once this returns. */
	void HandleConfirmAction();

	// Description: says what the highlighted entry does on the screen's Description Text, or stops.
	void SayOnScreen(const FText& Description);
	void StopSayingOnScreen();

	TWeakObjectPtr<UModularActivatableWidget> DescriptionScreen;
	TWeakObjectPtr<UModularActivatableWidget> DescribedScreen;

	TWeakObjectPtr<UWidget> AnchorWidget;
	FBox2D AnchorBox = FBox2D(ForceInit);
	bool bHasAnchor = false;

	/** Where the cursor was as the menu was anchored, for At Cursor, so the menu stays there as its size changes. */
	TOptional<FVector2D> OpeningCursor;

	TArray<FMadeEntry> MadeEntries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonButtonBase>> MadeEntryButtons;

	/** The entry highlighted now. Kept as the menu closes. */
	TWeakObjectPtr<UCommonButtonBase> HighlightedEntry;

	// Prompt
	FModularPromptView PromptView;
	bool bPrompting = false;
	bool bPromptShown = false;
	EModularPromptPresentation ShownPresentation = EModularPromptPresentation::Inline;
	TFunction<void(bool, int32)> PromptAnswer;
	TWeakObjectPtr<UModularPromptWidget> PromptDialog;

	/** The entry that asked, highlighted again as the prompt goes. */
	TWeakObjectPtr<UCommonButtonBase> PromptEntry;

	/** How the designer left the panels, to show them again as they were. */
	ESlateVisibility EntriesVisibility = ESlateVisibility::SelfHitTestInvisible;
	ESlateVisibility PromptPanelVisibility = ESlateVisibility::SelfHitTestInvisible;
	ESlateVisibility PromptBackdropVisibility = ESlateVisibility::Visible;

	FTSTicker::FDelegateHandle PlaceHandle;
	FTSTicker::FDelegateHandle HoverHandle;
};
