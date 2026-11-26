#pragma once

#include "CoreMinimal.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "Containers/Ticker.h"

#include "ModularUITransition.h"

#include "ModularPageSwitcher.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularPageShownSignature, int32, PageIndex, UWidget*, Page);
DECLARE_MULTICAST_DELEGATE_TwoParams(FModularPageChangingSignature, UWidget* /*LeavingPage*/, UWidget* /*NewPage*/);


/**
 * The pages of a screen, such as the pages of a tab list: it changes page at once, and brings the new page in with Page Transition, a quick
 * fade and a short slide from the side it comes from, as console games turn the pages of their menus. The transition plays in real time,
 * from the core ticker, so it plays while the game is paused, and the page coming in takes no clicks or hover until it's in, so nothing it
 * slides under a still cursor lights up on the way.
 * It's a Common Activatable Widget Switcher, so pages that are activatable widgets are activated as they show, and it links to a tab list as
 * any Common Animated Switcher does. Without a Page Transition, it changes pages with the switcher transition under Transition instead.
 */
UCLASS(meta = (DisplayName = "Modular Page Switcher"))
class MODULARUI_API UModularPageSwitcher : public UCommonActivatableWidgetSwitcher
{
	GENERATED_BODY()

public:
	UModularPageSwitcher(const FObjectInitializer& ObjectInitializer);

	//~ Begin UWidgetSwitcher Interface
	virtual void SetActiveWidgetIndex(int32 Index) override;
	virtual void SetActiveWidget(UWidget* Widget) override;
	//~ End UWidgetSwitcher Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	/** Whether a page is still coming in with Page Transition. */
	UFUNCTION(BlueprintPure, Category = "Page Switcher")
	bool IsPageTransitionPlaying() const { return ShowingPage.IsValid(); }

	/**
	 * Off, pages change at once, without Page Transition or the switcher transition, such as for the page a screen opens on, which its own
	 * open brings in. On by default.
	 */
	UFUNCTION(BlueprintCallable, Category = "Page Switcher")
	void SetPageTransitionsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Page Switcher")
	bool ArePageTransitionsEnabled() const { return bPageTransitionsEnabled; }

	/** A page has come in: its Page Transition ended, or it showed at once. */
	UPROPERTY(BlueprintAssignable, Category = "Page Switcher")
	FModularPageShownSignature OnPageShown;

	/** Just before the page changes, while the page being left is still drawn, such as to remember where focus was on it. */
	FModularPageChangingSignature OnPageChanging;

protected:
	/**
	 * How each new page comes in, from hidden, once it's switched to. With Slide From set to From Origin, a later page slides in from the
	 * side after it along Page Order, such as from the right, and an earlier page from the side before it, as if the pages lay side by side.
	 * Keep it short, around a tenth to a fifth of a second, with Ease Out, so the page is readable at once. No effect uses the switcher
	 * transition under Transition instead.
	 */
	UPROPERTY(EditAnywhere, Category = "Page Transition")
	FModularUITransition PageTransition;

	/** Which way the pages lie for a slide From Origin: side by side, as for tabs along the top, or one above another. */
	UPROPERTY(EditAnywhere, Category = "Page Transition")
	TEnumAsByte<EOrientation> PageOrder = Orient_Horizontal;

	/** The page coming in takes no clicks or hover until its transition ends, so the cursor doesn't light up what slides under it. */
	UPROPERTY(EditAnywhere, Category = "Page Transition")
	bool bBlockInputWhileShowing = true;

private:
	/** Before the switcher changes to New Index: says so while the old page is still drawn, and picks whose transition plays. Returns the index shown until now. */
	int32 BeginPageChange(int32 NewIndex);

	/** After the switcher changed: plays Page Transition on the new page, or says it's shown. */
	void EndPageChange(int32 PreviousIndex, int32 NewIndex);

	void StartPageTransition(UWidget& Page, int32 Direction);

	/** Ends a transition still playing where it ends, and gives the page its input back. With bNotify, On Page Shown says so. */
	void FinishPageTransition(bool bNotify);

	bool UsesPageTransition() const { return PageTransition.HasEffect(); }

	FModularUITransitionPlayer Player;
	FTSTicker::FDelegateHandle TickHandle;

	/** The page coming in, and its visibility, which it gets back once it's in. */
	TWeakObjectPtr<UWidget> ShowingPage;
	ESlateVisibility ShowingPageVisibility = ESlateVisibility::SelfHitTestInvisible;

	/** The page index shown, once it has changed. Until then, the switcher's own. */
	int32 ShownIndex = INDEX_NONE;

	bool bPageTransitionsEnabled = true;
};
