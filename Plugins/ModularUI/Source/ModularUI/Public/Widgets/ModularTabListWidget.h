#pragma once

#include "CoreMinimal.h"

#include "CommonTabListWidgetBase.h"
#include "Containers/Ticker.h"

#include "ModularTabListWidget.generated.h"

class UCommonButtonBase;
class UModularTabButton;
class UPanelWidget;
class UTexture2D;


/** One tab of a Modular Tab List, and the page it opens. */
USTRUCT(BlueprintType)
struct MODULARUI_API FModularTabDescriptor
{
	GENERATED_BODY()

	/** What Select Tab By ID and On Tab Selected know the tab as. None names it by its index, such as Tab0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tab")
	FName TabId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tab")
	FText Label;

	/** None makes a text-only tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tab")
	TObjectPtr<UTexture2D> Icon;

	/**
	 * Where focus goes as the tab's page shows, the first time or when the widget focused last is gone: the widget with this name in the
	 * page, in the page's own designer when it's a widget of its own. None leaves it to the page: an activatable page's desired focus
	 * target, or nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tab")
	FName FocusWidget;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularTabPageShownSignature, FName, TabId, UWidget*, Page);


/**
 * Common UI's tab list, set up in the designer: a Tab Button Class button in Tab Button Panel for each of Tabs, opening the page at the same
 * index in the linked switcher. Link a Common Animated Switcher holding the pages with Set Linked Switcher. The tab list's Next Tab and
 * Previous Tab input actions change tabs.
 * As a page shows again, focus goes back to where it was when the player left it, so a gamepad player picks up where they were. A
 * switcher with a Transition Duration keeps the old page up long enough to see where focus was; without one, focus goes to the tab's Focus
 * Widget instead.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class MODULARUI_API UModularTabListWidget : public UCommonTabListWidgetBase
{
	GENERATED_BODY()

public:
	/** The open tab's index in Tabs, or -1. */
	UFUNCTION(BlueprintPure, Category = "Tab List")
	int32 GetOpenTabIndex() const;

	/** The tab's index in Tabs, or -1. */
	UFUNCTION(BlueprintPure, Category = "Tab List")
	int32 GetTabIndexOf(FName TabId) const;

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	bool OpenTabAtIndex(int32 Index, bool bSuppressClickFeedback = false);

	/** The page the tab opens in the linked switcher, or null. */
	UFUNCTION(BlueprintPure, Category = "Tab List")
	UWidget* GetTabPage(FName TabId) const;

	const FDataTableRowHandle& GetNextTabAction() const { return NextTabInputActionData; }
	const FDataTableRowHandle& GetPreviousTabAction() const { return PreviousTabInputActionData; }

	/** A tab's page is drawn after it was opened, and focus has gone into it. For what else should happen as a page shows, such as aiming at an item. */
	UPROPERTY(BlueprintAssignable, Category = "Tab List")
	FModularTabPageShownSignature OnTabPageShown;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonTabListWidgetBase Interface
	virtual void HandlePostLinkedSwitcherChanged() override;
	virtual void HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton) override;
	//~ End UCommonTabListWidgetBase Interface

	/** In the order of the pages they open. */
	UPROPERTY(EditAnywhere, Category = "Tab List", meta = (TitleProperty = "Label"))
	TArray<FModularTabDescriptor> Tabs;

	UPROPERTY(EditAnywhere, Category = "Tab List")
	TSubclassOf<UModularTabButton> TabButtonClass;

	/** Space around each tab button, such as to keep tabs apart. For a Tab Button Panel that's a Horizontal Box or a Vertical Box. */
	UPROPERTY(EditAnywhere, Category = "Tab List")
	FMargin TabPadding;

	/**
	 * As a page shows, focus goes into it: back where it was when the player left that tab, or else to the tab's Focus Widget. Off, focus
	 * is left to the pages.
	 */
	UPROPERTY(EditAnywhere, Category = "Tab List")
	bool bFocusPages = true;

	/** Holds the tab buttons, such as a Horizontal Box. Leave it empty: they're made for it, and drawn in the designer with the first open. */
	UPROPERTY(BlueprintReadOnly, Category = "Tab List", meta = (BindWidget))
	TObjectPtr<UPanelWidget> TabButtonPanel;

private:
	UFUNCTION()
	void HandleTabSelected(FName TabId);

	FName MakeTabId(int32 Index) const;
	void RegisterTabs();
	void AddToPanel(UCommonButtonBase& TabButton);

	/** Where focus goes as the tab's page shows: where it was left, else the tab's Focus Widget, else the page's own target. */
	UWidget* FindFocusTarget(FName TabId, UWidget& Page) const;

#if WITH_EDITOR
	void RefreshDesignerPreview();
#endif

	/** The tab whose page shows now, and the widget focused last on each tab's page. */
	FName ShownTabId;
	TMap<FName, TWeakObjectPtr<UWidget>> LastFocused;
	FTSTicker::FDelegateHandle PageShownHandle;
};
