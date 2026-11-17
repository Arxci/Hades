#pragma once

#include "CoreMinimal.h"

#include "CommonTabListWidgetBase.h"

#include "HadesTabListWidget.generated.h"

class UCommonButtonBase;
class UHadesTabButton;
class UPanelWidget;
class UTexture2D;


/** One tab of a Hades Tab List. */
USTRUCT(BlueprintType)
struct FHadesTabDescriptor
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
};


/**
 * Common UI's tab list, set up in the designer: a Tab Button Class button in Tab Button Panel for each of Tabs, opening the page at the same
 * index in the linked switcher. Link a Common Animated Switcher holding the pages with Set Linked Switcher. The tab list's Next Tab and
 * Previous Tab input actions change tabs.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HADES_API UHadesTabListWidget : public UCommonTabListWidgetBase
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

	const FDataTableRowHandle& GetNextTabAction() const { return NextTabInputActionData; }
	const FDataTableRowHandle& GetPreviousTabAction() const { return PreviousTabInputActionData; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	//~ Begin UCommonTabListWidgetBase Interface
	virtual void HandlePostLinkedSwitcherChanged() override;
	virtual void HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton) override;
	//~ End UCommonTabListWidgetBase Interface

	/** In the order of the pages they open. */
	UPROPERTY(EditAnywhere, Category = "Tab List", meta = (TitleProperty = "Label"))
	TArray<FHadesTabDescriptor> Tabs;

	UPROPERTY(EditAnywhere, Category = "Tab List")
	TSubclassOf<UHadesTabButton> TabButtonClass;

	/** Space around each tab button, such as to keep tabs apart. For a Tab Button Panel that's a Horizontal Box or a Vertical Box. */
	UPROPERTY(EditAnywhere, Category = "Tab List")
	FMargin TabPadding;

	/** Holds the tab buttons, such as a Horizontal Box. Leave it empty: they're made for it, and drawn in the designer with the first open. */
	UPROPERTY(BlueprintReadOnly, Category = "Tab List", meta = (BindWidget))
	TObjectPtr<UPanelWidget> TabButtonPanel;

private:
	FName GetTabId(int32 Index) const;
	void RegisterTabs();
	void AddToPanel(UCommonButtonBase& TabButton);

#if WITH_EDITOR
	void RefreshDesignerPreview();
#endif
};
