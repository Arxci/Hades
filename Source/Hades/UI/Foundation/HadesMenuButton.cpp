#include "HadesMenuButton.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UHadesMenuButton::SetLabelText(const FText& InText)
{
	LabelText = InText;
	if (Label)
	{
		Label->SetText(LabelText);
	}
}

void UHadesMenuButton::SetIconTexture(UTexture2D* InTexture)
{
	IconTexture = InTexture;
	if (Icon && IconTexture)
	{
		// Keeps the designer's Image Size, whatever the texture's size.
		Icon->SetBrushFromTexture(IconTexture, /*bMatchSize*/ false);
	}

	const ESlateVisibility NewVisiblity = IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Icon)
	{
		Icon->SetVisibility(NewVisiblity);
	}
	if (Badge)
	{
		Badge->SetVisibility(NewVisiblity);
	}
}

void UHadesMenuButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetLabelText(LabelText);
	SetIconTexture(IconTexture);
	SetLit(false);
}

void UHadesMenuButton::NativeConstruct()
{
	Super::NativeConstruct();

	// Menus are reused, and a row can be left lit when its menu closed under the mouse.
	SetLit(false);
}

void UHadesMenuButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	SetLit(true);
}

void UHadesMenuButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	SetLit(false);
}

void UHadesMenuButton::SetLit(bool bInLit)
{
	const FSlateBrush& BadgeLook = bInLit ? LitBadgeBrush : BadgeBrush;
	if (Badge && BadgeLook.GetResourceObject())
	{
		Badge->SetBrush(BadgeLook);
	}
	if (Icon)
	{
		Icon->SetColorAndOpacity(bInLit ? LitIconColor : IconColor);
	}
}
