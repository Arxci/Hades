#include "ModularUIFocus.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/PlatformTime.h"
#include "Layout/WidgetPath.h"
#include "Slate/SObjectWidget.h"

bool ModularUI::IsWidgetDrawn(const UWidget& Widget)
{
	const TSharedPtr<SWidget> SlateWidget = Widget.GetCachedWidget();
	if (!SlateWidget.IsValid() || !Widget.IsVisible() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	// Only widgets drawn now are found from the window down, with every widget on the way shown.
	FWidgetPath Path;
	return FSlateApplication::Get().FindPathToWidget(SlateWidget.ToSharedRef(), Path) && Path.IsValid();
}

void ModularUI::CallWhenDrawn(FTSTicker::FDelegateHandle& Handle, UObject& Owner, UWidget* Target, TFunction<void(UWidget&)> Callback, float Timeout)
{
	CancelCallWhenDrawn(Handle);
	if (!Target || !Callback)
	{
		return;
	}

	// Checked from the next frame, once whatever just changed has been laid out. The handle belongs to Owner, which the weak lambda outlives
	// only by never running again.
	const TWeakObjectPtr<UWidget> WeakTarget = Target;
	const double GiveUpTime = FPlatformTime::Seconds() + FMath::Max(0.0f, Timeout);
	FTSTicker::FDelegateHandle* HandlePtr = &Handle;
	Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(&Owner, [HandlePtr, WeakTarget, Callback = MoveTemp(Callback), GiveUpTime](float)
	{
		UWidget* Drawn = WeakTarget.Get();
		const bool bTimedOut = FPlatformTime::Seconds() > GiveUpTime;
		if (Drawn && !bTimedOut && !IsWidgetDrawn(*Drawn))
		{
			return true;
		}

		*HandlePtr = FTSTicker::FDelegateHandle();
		if (Drawn && !bTimedOut)
		{
			Callback(*Drawn);
		}
		return false;
	}));
}

void ModularUI::CancelCallWhenDrawn(FTSTicker::FDelegateHandle& Handle)
{
	if (Handle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Handle);
	}
	Handle = FTSTicker::FDelegateHandle();
}

UWidget* ModularUI::FindFocusedWidgetIn(const UWidget& Root, const ULocalPlayer* LocalPlayer)
{
	const TSharedPtr<SWidget> RootSlate = Root.GetCachedWidget();
	ULocalPlayer* MutablePlayer = const_cast<ULocalPlayer*>(LocalPlayer);
	const TSharedPtr<FSlateUser> SlateUser = MutablePlayer && FSlateApplication::IsInitialized() ? MutablePlayer->GetSlateUser() : TSharedPtr<FSlateUser>();
	const TSharedPtr<SWidget> Focused = SlateUser ? SlateUser->GetFocusedWidget() : TSharedPtr<SWidget>();
	if (!RootSlate.IsValid() || !Focused.IsValid())
	{
		return nullptr;
	}

	FWidgetPath Path;
	if (!FSlateApplication::Get().FindPathToWidget(Focused.ToSharedRef(), Path))
	{
		return nullptr;
	}

	// The deepest user widget on the way to the focused Slate widget, below Root: Common UI's buttons are user widgets, so this is the
	// button, or the cell, rather than the Slate button inside it.
	static const FName ObjectWidgetType(TEXT("SObjectWidget"));
	bool bInsideRoot = false;
	UWidget* Found = nullptr;
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const TSharedRef<SWidget>& SlateWidget = Path.Widgets[Index].Widget;
		if (&SlateWidget.Get() == RootSlate.Get())
		{
			bInsideRoot = true;
		}
		if (bInsideRoot && SlateWidget->GetType() == ObjectWidgetType)
		{
			Found = StaticCastSharedRef<SObjectWidget>(SlateWidget)->GetWidgetObject();
		}
	}
	return Found;
}

UWidget* ModularUI::FindWidgetByName(UWidget& Root, FName Name)
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (Root.GetFName() == Name)
	{
		return &Root;
	}
	if (const UUserWidget* UserWidget = Cast<UUserWidget>(&Root))
	{
		return UserWidget->WidgetTree ? UserWidget->WidgetTree->FindWidget(Name) : nullptr;
	}

	UWidget* Found = nullptr;
	UWidgetTree::ForWidgetAndChildren(&Root, [&Found, Name](UWidget* Widget)
	{
		if (!Found && Widget && Widget->GetFName() == Name)
		{
			Found = Widget;
		}
	});
	return Found;
}
