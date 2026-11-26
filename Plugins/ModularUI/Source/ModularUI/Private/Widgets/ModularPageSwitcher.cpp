#include "Widgets/ModularPageSwitcher.h"

#include "Components/Widget.h"

UModularPageSwitcher::UModularPageSwitcher(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// A quick fade and a short slide, from the side the page comes from, settling gently.
	PageTransition.Duration = 0.15f;
	PageTransition.Easing = EEasingFunc::EaseOut;
	PageTransition.EasingStrength = 3.0f;
	PageTransition.bFade = true;
	PageTransition.FadeFrom = 0.0f;
	PageTransition.bSlide = true;
	PageTransition.SlideDistance = 40.0f;
	PageTransition.SlideFrom = EModularUISlide::FromOrigin;
	PageTransition.bScale = false;
}

void UModularPageSwitcher::SetActiveWidgetIndex(int32 Index)
{
	const int32 PreviousIndex = BeginPageChange(Index);
	Super::SetActiveWidgetIndex(Index);
	EndPageChange(PreviousIndex, Index);
}

void UModularPageSwitcher::SetActiveWidget(UWidget* Widget)
{
	// Tab lists change pages by widget, which the base doesn't send through Set Active Widget Index.
	const int32 NewIndex = Widget ? GetChildIndex(Widget) : INDEX_NONE;
	const int32 PreviousIndex = BeginPageChange(NewIndex);
	Super::SetActiveWidget(Widget);
	EndPageChange(PreviousIndex, NewIndex);
}

void UModularPageSwitcher::ReleaseSlateResources(bool bReleaseChildren)
{
	FinishPageTransition(/*bNotify*/ false);

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UModularPageSwitcher::SetPageTransitionsEnabled(bool bEnabled)
{
	bPageTransitionsEnabled = bEnabled;
	if (!bEnabled)
	{
		FinishPageTransition(/*bNotify*/ true);
	}
}

int32 UModularPageSwitcher::BeginPageChange(int32 NewIndex)
{
	const int32 PreviousIndex = ShownIndex != INDEX_NONE ? ShownIndex : GetActiveWidgetIndex();
	if (NewIndex != PreviousIndex && GetWidgetAtIndex(NewIndex))
	{
		// While the page being left is still drawn, then the page still coming in, if any, is in at once.
		OnPageChanging.Broadcast(GetWidgetAtIndex(PreviousIndex), GetWidgetAtIndex(NewIndex));
		FinishPageTransition(/*bNotify*/ true);
	}

	// Page Transition, or the switcher transition, never both, and neither while transitions are off.
	SetDisableTransitionAnimation(UsesPageTransition() || !bPageTransitionsEnabled);
	return PreviousIndex;
}

void UModularPageSwitcher::EndPageChange(int32 PreviousIndex, int32 NewIndex)
{
	UWidget* Page = GetWidgetAtIndex(NewIndex);
	if (!Page || NewIndex == PreviousIndex)
	{
		return;
	}

	ShownIndex = NewIndex;
	if (IsDesignTime() || !bPageTransitionsEnabled || !UsesPageTransition())
	{
		OnPageShown.Broadcast(NewIndex, Page);
		return;
	}
	StartPageTransition(*Page, PreviousIndex == INDEX_NONE ? 0 : NewIndex > PreviousIndex ? 1 : -1);
}

void UModularPageSwitcher::StartPageTransition(UWidget& Page, int32 Direction)
{
	// From the side the page comes from: a later page from after it along Page Order, an earlier one from before it. A slide From Origin
	// starts on the side opposite Away, so Away points back the way the pages go. The first page shown has no side, and comes up.
	const FVector2D Along = PageOrder == Orient_Vertical ? FVector2D(0.0, 1.0) : FVector2D(1.0, 0.0);
	const FVector2D Away = Direction == 0 ? FVector2D(0.0, -1.0) : -Along * static_cast<double>(Direction);

	ShowingPage = &Page;
	ShowingPageVisibility = Page.GetVisibility();
	if (bBlockInputWhileShowing)
	{
		Page.SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Adds to the page's own render opacity and transform, which it always has back by now.
	Player.Init(&Page, &Page);
	Player.SetOrigin(Away, FVector2D(0.5, 0.5));
	Player.PlayIn(PageTransition);
	if (!Player.IsPlaying())
	{
		FinishPageTransition(/*bNotify*/ true);
		return;
	}

	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
		{
			if (Player.Advance(DeltaTime))
			{
				return true;
			}

			// Cleared first, as finishing can change pages again, with a ticker of its own.
			TickHandle = FTSTicker::FDelegateHandle();
			FinishPageTransition(/*bNotify*/ true);
			return false;
		}));
	}
}

void UModularPageSwitcher::FinishPageTransition(bool bNotify)
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle = FTSTicker::FDelegateHandle();
	}

	UWidget* Page = ShowingPage.Get();
	ShowingPage.Reset();
	if (!Page)
	{
		return;
	}

	Player.Finish();
	if (bBlockInputWhileShowing)
	{
		Page->SetVisibility(ShowingPageVisibility);
	}
	if (bNotify)
	{
		OnPageShown.Broadcast(GetChildIndex(Page), Page);
	}
}
