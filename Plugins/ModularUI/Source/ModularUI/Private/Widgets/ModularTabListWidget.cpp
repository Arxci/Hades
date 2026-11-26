#include "Widgets/ModularTabListWidget.h"

#include "CommonActivatableWidget.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Input/CommonUIInputTypes.h"

#include "ModularActivatableWidget.h"
#include "ModularUI.h"
#include "ModularUIFocus.h"
#include "Widgets/ModularPageSwitcher.h"
#include "Widgets/ModularTabButton.h"

int32 UModularTabListWidget::GetOpenTabIndex() const
{
	return GetTabIndexOf(GetActiveTab());
}

bool UModularTabListWidget::OpenTabAtIndex(int32 Index, bool bSuppressClickFeedback)
{
	return Tabs.IsValidIndex(Index) && SelectTabByID(MakeTabId(Index), bSuppressClickFeedback);
}

FName UModularTabListWidget::MakeTabId(int32 Index) const
{
	return Tabs[Index].TabId.IsNone() ? FName(*FString::Printf(TEXT("Tab%d"), Index)) : Tabs[Index].TabId;
}

int32 UModularTabListWidget::GetTabIndexOf(FName TabId) const
{
	for (int32 Index = 0; !TabId.IsNone() && Index < Tabs.Num(); ++Index)
	{
		if (MakeTabId(Index) == TabId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

UWidget* UModularTabListWidget::GetTabPage(FName TabId) const
{
	const UCommonAnimatedSwitcher* Switcher = const_cast<UModularTabListWidget*>(this)->GetLinkedSwitcher();
	const int32 Index = GetTabIndexOf(TabId);
	return Switcher && Index != INDEX_NONE ? Switcher->GetWidgetAtIndex(Index) : nullptr;
}

void UModularTabListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnTabSelected.AddUniqueDynamic(this, &ThisClass::HandleTabSelected);

	// Each tab's own key, bound while the screen the tab list is on is active, as Next Tab and Previous Tab are.
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		if (Tabs[Index].Shortcut.IsValid())
		{
			RegisterUIActionBinding(FBindUIActionArgs(Tabs[Index].Shortcut, /*bShowInActionBar*/ false,
				FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTabShortcut, Index)));
		}
	}
}

void UModularTabListWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

#if WITH_EDITOR
	if (IsDesignTime())
	{
		RefreshDesignerPreview();
	}
#endif
}

void UModularTabListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegisterTabs();
}

void UModularTabListWidget::NativeDestruct()
{
	ModularUI::CancelCallWhenDrawn(PageShownHandle);

	Super::NativeDestruct();
}

void UModularTabListWidget::HandlePreLinkedSwitcherChanged()
{
	if (UModularPageSwitcher* PageSwitcher = Cast<UModularPageSwitcher>(GetLinkedSwitcher()))
	{
		PageSwitcher->OnPageChanging.RemoveAll(this);
	}

	Super::HandlePreLinkedSwitcherChanged();
}

void UModularTabListWidget::HandlePostLinkedSwitcherChanged()
{
	// A Modular Page Switcher changes page at once, so it says where focus was on a page just before it goes.
	if (UModularPageSwitcher* PageSwitcher = Cast<UModularPageSwitcher>(GetLinkedSwitcher()))
	{
		PageSwitcher->OnPageChanging.RemoveAll(this);
		PageSwitcher->OnPageChanging.AddUObject(this, &ThisClass::HandlePageChanging);
	}

	// Linked before it's built, it registers its tabs as it's constructed.
	if (!IsDesignTime() && GetCachedWidget().IsValid())
	{
		RegisterTabs();
	}

	Super::HandlePostLinkedSwitcherChanged();
}

void UModularTabListWidget::RegisterTabs()
{
	UCommonAnimatedSwitcher* Switcher = GetLinkedSwitcher();
	if (!Switcher || Tabs.IsEmpty())
	{
		return;
	}
	if (!TabButtonClass)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s has no Tab Button Class, so it shows no tabs."), *GetPathNameSafe(this));
		return;
	}

	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const FName TabId = MakeTabId(Index);
		UWidget* Page = Switcher->GetWidgetAtIndex(Index);
		if (!Page)
		{
			UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s: the linked switcher has no page %d for the tab %s."), *GetPathNameSafe(this), Index, *TabId.ToString());
		}
		else if (!GetTabButtonBaseByID(TabId))
		{
			RegisterTab(TabId, TabButtonClass, Page);
		}
	}
}

void UModularTabListWidget::HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton)
{
	Super::HandleTabCreation_Implementation(TabId, TabButton);

	const int32 Index = GetTabIndexOf(TabId);
	UModularTabButton* ModularTab = Cast<UModularTabButton>(TabButton);
	if (ModularTab && Tabs.IsValidIndex(Index))
	{
		ModularTab->SetLabelText(Tabs[Index].Label);
		ModularTab->SetIconTexture(Tabs[Index].Icon);
		ModularTab->SetDescription(Tabs[Index].Description);
	}
	if (TabButton)
	{
		AddToPanel(*TabButton);
	}
}

void UModularTabListWidget::AddToPanel(UCommonButtonBase& TabButton)
{
	UPanelSlot* ButtonSlot = TabButtonPanel ? TabButtonPanel->AddChild(&TabButton) : nullptr;
	if (UHorizontalBoxSlot* InRow = Cast<UHorizontalBoxSlot>(ButtonSlot))
	{
		InRow->SetPadding(TabPadding);
	}
	else if (UVerticalBoxSlot* InColumn = Cast<UVerticalBoxSlot>(ButtonSlot))
	{
		InColumn->SetPadding(TabPadding);
	}
}

void UModularTabListWidget::HandleTabShortcut(int32 Index)
{
	// Nothing while the screen goes. Pressed again on its own open tab, it closes the screen, as the key that opened it there would.
	UModularActivatableWidget* Screen = ModularUI::FindEnclosing<UModularActivatableWidget>(*this);
	if (!Tabs.IsValidIndex(Index) || (Screen && Screen->IsClosing()))
	{
		return;
	}
	if (GetOpenTabIndex() != Index)
	{
		OpenTabAtIndex(Index);
	}
	else if (bShortcutTogglesScreen && Screen)
	{
		Screen->CloseWidget();
	}
}

void UModularTabListWidget::HandlePageChanging(UWidget* LeavingPage, UWidget* NewPage)
{
	RememberFocusOnShownTab();
}

void UModularTabListWidget::RememberFocusOnShownTab()
{
	UWidget* ShownPage = !ShownTabId.IsNone() ? GetTabPage(ShownTabId) : nullptr;
	if (UWidget* Focused = ShownPage ? ModularUI::FindFocusedWidgetIn(*ShownPage, GetOwningLocalPlayer()) : nullptr)
	{
		LastFocused.Add(ShownTabId, Focused);
	}
}

void UModularTabListWidget::HandleTabSelected(FName TabId)
{
	// Where focus is on the page being left, if it's still up, as an animated switcher keeps it until its transition is half done. A Modular
	// Page Switcher said so already, just before it changed pages, and nothing found now replaces that.
	if (!ShownTabId.IsNone() && ShownTabId != TabId)
	{
		RememberFocusOnShownTab();
	}
	ShownTabId = TabId;

	// Once the new page is drawn, focus goes into it, and whoever listens hears it's shown. However long the switcher takes to bring it in.
	UWidget* Page = GetTabPage(TabId);
	ModularUI::CallWhenDrawn(PageShownHandle, *this, Page, [this, TabId](UWidget& Shown)
	{
		if (ShownTabId != TabId)
		{
			return;
		}
		if (UWidget* Target = bFocusPages ? FindFocusTarget(TabId, Shown) : nullptr)
		{
			Target->SetFocus();
		}
		OnTabPageShown.Broadcast(TabId, &Shown);
	}, ModularUI::PageDrawnTimeout);
}

UWidget* UModularTabListWidget::FindFocusTarget(FName TabId, UWidget& Page) const
{
	const TWeakObjectPtr<UWidget>* Remembered = LastFocused.Find(TabId);
	UWidget* Last = Remembered ? Remembered->Get() : nullptr;
	if (Last && Last->IsVisible() && Last->GetIsEnabled() && ModularUI::IsWidgetDrawn(*Last))
	{
		return Last;
	}

	const int32 Index = GetTabIndexOf(TabId);
	if (UWidget* Named = Tabs.IsValidIndex(Index) ? ModularUI::FindWidgetByName(Page, Tabs[Index].FocusWidget) : nullptr)
	{
		return Named;
	}

	const UCommonActivatableWidget* ActivatablePage = Cast<UCommonActivatableWidget>(&Page);
	return ActivatablePage ? ActivatablePage->GetDesiredFocusTarget() : nullptr;
}

#if WITH_EDITOR
void UModularTabListWidget::RefreshDesignerPreview()
{
	if (!TabButtonPanel)
	{
		return;
	}

	// Built afresh each time, as the designer runs this again whenever the widget changes.
	TabButtonPanel->ClearChildren();
	for (int32 Index = 0; TabButtonClass && Index < Tabs.Num(); ++Index)
	{
		UModularTabButton* Preview = CreateWidget<UModularTabButton>(this, TabButtonClass);
		if (!Preview)
		{
			continue;
		}
		Preview->SetDesignerFlags(GetDesignerFlags());
		Preview->SetLabelText(Tabs[Index].Label);
		Preview->SetIconTexture(Tabs[Index].Icon);
		Preview->SetIsSelectable(true);
		Preview->SetIsSelected(Index == 0, false);
		AddToPanel(*Preview);
	}
}
#endif
