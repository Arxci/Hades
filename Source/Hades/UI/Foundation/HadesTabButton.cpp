#include "HadesTabButton.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

UHadesTabButton::UHadesTabButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Tabs change with their keys, so gamepad focus stays on the page.
	SetIsFocusable(false);
	Justification = EHadesMenuButtonJustify::Center;
}

void UHadesTabButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshTab();
}

void UHadesTabButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	RefreshTab();
}

void UHadesTabButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	RefreshTab();
}

void UHadesTabButton::RefreshTab()
{
	const bool bOpen = GetSelected();
	if (Icon)
	{
		Icon->SetRenderScale(FVector2D(bOpen ? SelectedIconScale : 1.0f));
	}
	if (Label)
	{
		// Hidden, not collapsed, so the tab bar doesn't shift as tabs change.
		const bool bShowLabel = bOpen || bAlwaysShowLabel || !IconTexture;
		Label->SetVisibility(bShowLabel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (SelectedFrame)
	{
		SelectedFrame->SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
