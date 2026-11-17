#include "HadesTabListWidget.h"

#include "CommonAnimatedSwitcher.h"
#include "CommonButtonBase.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

#include "Hades/Hades.h"
#include "Hades/UI/Foundation/HadesTabButton.h"

int32 UHadesTabListWidget::GetOpenTabIndex() const
{
	return GetTabIndexOf(GetActiveTab());
}

bool UHadesTabListWidget::OpenTabAtIndex(int32 Index, bool bSuppressClickFeedback)
{
	return Tabs.IsValidIndex(Index) && SelectTabByID(GetTabId(Index), bSuppressClickFeedback);
}

FName UHadesTabListWidget::GetTabId(int32 Index) const
{
	return Tabs[Index].TabId.IsNone() ? FName(*FString::Printf(TEXT("Tab%d"), Index)) : Tabs[Index].TabId;
}

int32 UHadesTabListWidget::GetTabIndexOf(FName TabId) const
{
	for (int32 Index = 0; !TabId.IsNone() && Index < Tabs.Num(); ++Index)
	{
		if (GetTabId(Index) == TabId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UHadesTabListWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

#if WITH_EDITOR
	if (IsDesignTime())
	{
		RefreshDesignerPreview();
	}
#endif
}

void UHadesTabListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RegisterTabs();
}

void UHadesTabListWidget::HandlePostLinkedSwitcherChanged()
{
	// Linked before it's built, it registers its tabs as it's constructed.
	if (!IsDesignTime() && GetCachedWidget().IsValid())
	{
		RegisterTabs();
	}

	Super::HandlePostLinkedSwitcherChanged();
}

void UHadesTabListWidget::RegisterTabs()
{
	UCommonAnimatedSwitcher* Switcher = GetLinkedSwitcher();
	if (!Switcher || Tabs.IsEmpty())
	{
		return;
	}
	if (!TabButtonClass)
	{
		UE_LOG(LogHadesUI, Warning, TEXT("[Tabs] %s has no Tab Button Class, so it shows no tabs."), *GetPathNameSafe(this));
		return;
	}

	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const FName TabId = GetTabId(Index);
		UWidget* Page = Switcher->GetWidgetAtIndex(Index);
		if (!Page)
		{
			UE_LOG(LogHadesUI, Warning, TEXT("[Tabs] %s: the linked switcher has no page %d for the tab %s."), *GetPathNameSafe(this), Index, *TabId.ToString());
		}
		else if (!GetTabButtonBaseByID(TabId))
		{
			RegisterTab(TabId, TabButtonClass, Page);
		}
	}
}

void UHadesTabListWidget::HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton)
{
	Super::HandleTabCreation_Implementation(TabId, TabButton);

	const int32 Index = GetTabIndexOf(TabId);
	UHadesTabButton* HadesTab = Cast<UHadesTabButton>(TabButton);
	if (HadesTab && Tabs.IsValidIndex(Index))
	{
		HadesTab->SetLabelText(Tabs[Index].Label);
		HadesTab->SetIconTexture(Tabs[Index].Icon);
	}
	if (TabButton)
	{
		AddToPanel(*TabButton);
	}
}

void UHadesTabListWidget::AddToPanel(UCommonButtonBase& TabButton)
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

#if WITH_EDITOR
void UHadesTabListWidget::RefreshDesignerPreview()
{
	if (!TabButtonPanel)
	{
		return;
	}

	// Built afresh each time, as the designer runs this again whenever the widget changes.
	TabButtonPanel->ClearChildren();
	for (int32 Index = 0; TabButtonClass && Index < Tabs.Num(); ++Index)
	{
		UHadesTabButton* Preview = CreateWidget<UHadesTabButton>(this, TabButtonClass);
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
