#include "HadesTabButton.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UHadesTabButton::UHadesTabButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Tabs change with their keys, so gamepad focus stays on the page.
	SetIsFocusable(false);
}

void UHadesTabButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (Icon && IconTexture)
	{
		// Keeps the designer's Image Size, whatever the texture's size.
		Icon->SetBrushFromTexture(IconTexture, /*bMatchSize*/ false);
	}
	if (Icon)
	{
		Icon->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Label)
	{
		Label->SetText(LabelText);
	}
	SetActive(bActive);
}

void UHadesTabButton::SetActive(bool bInActive)
{
	bActive = bInActive;
	if (Icon)
	{
		Icon->SetRenderScale(FVector2D(bActive ? ActiveIconScale : 1.0f));
	}
	if (Label)
	{
		// Hidden, not collapsed, so the tab bar doesn't shift as tabs change.
		Label->SetVisibility(bActive || !IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (ActiveFrame)
	{
		ActiveFrame->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
