#pragma once

#include "CoreMinimal.h"

#include "Components/DynamicEntryBoxBase.h"
#include "Containers/Ticker.h"
#include "Input/UIActionBindingHandle.h"
#include "UObject/ObjectKey.h"

#include "ModularBoundActionBar.generated.h"

class UCommonButtonBase;
class UCommonUIActionRouterBase;
class UUserWidget;
enum class ECommonInputType : uint8;


/**
 * The actions the player can press now, each a button of Action Button Class with its key and name, as Common UI's Common Bound Action Bar
 * shows them: what's bound by the screens and menus taking input, and shown in action bars. Use it in place of a Common Bound Action Bar,
 * with the same Action Button Class.
 * It only changes what changed. A Common Bound Action Bar makes every button again whenever any binding changes, even one it doesn't show,
 * which shows as a flicker, such as when a menu opens over a screen. Here an action still shown keeps its button and its place, so a menu
 * whose Back and Confirm are bound as the screen's were changes nothing on screen, and an action that comes or goes leaves the rest alone.
 * A change waits until the bindings have settled for Settle Time, so the states Common UI passes through as menus open and close never show.
 * Actions keep the order they were shown in; new ones go after them, earlier only for a lower Nav Bar Priority, as Common UI sorts them.
 * Its buttons never take gamepad focus: a gamepad presses the key a button shows. With Clickable Buttons off, they don't take the mouse
 * either.
 */
UCLASS(meta = (DisplayName = "Modular Bound Action Bar"))
class MODULARUI_API UModularBoundActionBar : public UDynamicEntryBoxBase
{
	GENERATED_BODY()

public:
	UModularBoundActionBar(const FObjectInitializer& ObjectInitializer);

	/** Works out again what it shows, at once, without waiting for the bindings to settle. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	void RefreshActions();

	/** The actions shown, in order. */
	const TArray<FUIActionBindingHandle>& GetShownActions() const { return ShownHandles; }

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	//~ Begin UWidget Interface
	virtual void OnWidgetRebuilt() override;
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	/** What it shows changed, once the buttons are updated, such as to fade the bar in when it had nothing. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action Bar")
	void OnActionsChanged(int32 NumShown);

	/** Made for each action shown: a Common UI button that shows a bound action, such as a Common Bound Action Button. */
	UPROPERTY(EditAnywhere, Category = "Action Bar", meta = (MustImplement = "/Script/CommonUI.CommonBoundActionButtonInterface"))
	TSubclassOf<UCommonButtonBase> ActionButtonClass;

	/** An action bound more than once shows once, such as Back bound by a screen and a menu over it. As Common UI does. */
	UPROPERTY(EditAnywhere, Category = "Action Bar")
	bool bIgnoreDuplicateActions = true;

	/**
	 * A click or tap on a button does its action, as Common UI's own action bar does, which suits menus played with the mouse. Off, the
	 * buttons only show each action's key and name, as console games show them: the mouse goes through them to whatever is under them, and
	 * they never light up under it. A hold action's button never takes clicks either way, as only its key can hold it.
	 */
	UPROPERTY(EditAnywhere, Category = "Action Bar")
	bool bClickableButtons = true;

	/**
	 * How long, in seconds of real time, what's bound must stay the same before the bar shows it, so it skips what Common UI passes through
	 * for a frame or two as menus open and close. 0 shows each change on the next frame.
	 */
	UPROPERTY(EditAnywhere, Category = "Action Bar", meta = (ClampMin = "0", UIMax = "0.25", Units = "s"))
	float SettleTime = 0.05f;

#if WITH_EDITORONLY_DATA
	/** Buttons drawn in the designer, to see the bar laid out. */
	UPROPERTY(EditAnywhere, Category = "Action Bar", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NumDesignerPreviewEntries = 3;
#endif

private:
	/**
	 * What tells an action apart from others: its input action, or for a data table row, the row's name. Kept apart from how Common UI names
	 * bindings, so two input actions never count as one.
	 */
	struct FActionKey
	{
		FObjectKey InputAction;
		FName RowName;

		bool operator==(const FActionKey& Other) const { return InputAction == Other.InputAction && RowName == Other.RowName; }
		friend uint32 GetTypeHash(const FActionKey& ActionKey) { return HashCombine(GetTypeHash(ActionKey.InputAction), GetTypeHash(ActionKey.RowName)); }
	};

	/** One action shown: the binding it shows, what tells it apart from other actions, and its button. */
	struct FShownAction
	{
		FUIActionBindingHandle Handle;
		FActionKey Key;
		TWeakObjectPtr<UUserWidget> Button;
	};

	/** One action to show: its binding, what tells it apart, how Common UI would sort it, and where it showed before, or INDEX_NONE. */
	struct FWantedAction
	{
		FUIActionBindingHandle Handle;
		FActionKey Key;
		int32 Priority = 0;
		int32 ShownIndex = INDEX_NONE;
	};

	void Listen();
	void StopListening();

	void HandleBoundActionsUpdated();
	void HandleInputMethodChanged(ECommonInputType InputType);

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	/** Something bound changed: the bar updates once nothing has changed for Settle Time. */
	void QueueRefresh();
	void CancelQueuedRefresh();

	/** What to show now, in order: bindings shown in action bars that have a key for the input in use, each action once. */
	TArray<FWantedAction> GatherWantedActions() const;

	/** Shows Wanted, keeping the buttons of actions still shown where the order allows. */
	void ShowActions(const TArray<FWantedAction>& Wanted);

	UUserWidget* MakeButton(const FUIActionBindingHandle& Handle);
	void RemoveButton(const FShownAction& Shown);
	void ResetButtons();

	/** Lets the button take the mouse only while Clickable Buttons is on and its action isn't a hold, and never gamepad focus. */
	void ApplyButtonInput(UUserWidget& Button, const FUIActionBindingHandle& Handle) const;

#if WITH_EDITOR
	void RefreshDesignerPreview();
#endif

	TArray<FShownAction> Shown;
	TArray<FUIActionBindingHandle> ShownHandles;

	TWeakObjectPtr<UCommonUIActionRouterBase> ListenedRouter;
	FDelegateHandle BoundActionsHandle;
	FDelegateHandle InputMethodHandle;
	FTSTicker::FDelegateHandle RefreshHandle;

	/** When what's bound last changed, in platform seconds, which a queued refresh waits Settle Time past. */
	double LastChangeTime = 0.0;
};
