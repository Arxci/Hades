#include "Widgets/ModularButton.h"

#include "CommonTextBlock.h"
#include "Components/BorderSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UModularButton::SetLabelText(const FText& InText)
{
	LabelText = InText;
	if (Label)
	{
		Label->SetText(LabelText);
	}
}

void UModularButton::SetIconTexture(UTexture2D* InTexture)
{
	IconTexture = InTexture;
	if (Icon && IconTexture)
	{
		// Keeps the designer's Image Size, whatever the texture's size.
		Icon->SetBrushFromTexture(IconTexture, /*bMatchSize*/ false);
	}

	// What holds the icon beside the label, such as a Size Box, hides with it, so its space and padding go too and the label isn't pushed over.
	const ESlateVisibility Shown = IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Icon)
	{
		Icon->SetVisibility(Shown);
	}
	UWidget* IconInRow = FindIconInRow();
	if (IconInRow && IconInRow != Icon)
	{
		IconInRow->SetVisibility(Shown);
	}
}

UWidget* UModularButton::FindIconInRow() const
{
	const UPanelWidget* Row = Label ? Label->GetParent() : nullptr;
	UWidget* InRow = Icon;
	while (InRow && Row && InRow->GetParent() != Row)
	{
		InRow = InRow->GetParent();
	}
	return InRow ? InRow : Icon.Get();
}

void UModularButton::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetLabelText(LabelText);
	SetIconTexture(IconTexture);
	SetHoverLit(false);
	ApplyTextStyle();
	ApplyJustification();
}

void UModularButton::NativeConstruct()
{
	Super::NativeConstruct();

	// Menus are reused, and a row can be left lit when its menu closed under the mouse.
	SetHoverLit(false);
	ApplyTextStyle();
	ApplyJustification();
}

void UModularButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	SetHoverLit(true);
}

void UModularButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	SetHoverLit(false);
}

void UModularButton::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);

	RefreshIconColor();
}

void UModularButton::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);

	RefreshIconColor();
}

void UModularButton::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	ApplyTextStyle();
}

void UModularButton::SetHoverLit(bool bInHoverLit)
{
	bHoverLit = bInHoverLit;
	RefreshIconColor();
}

void UModularButton::RefreshIconColor()
{
	if (Icon)
	{
		Icon->SetColorAndOpacity(bHoverLit || GetSelected() ? LitIconColor : IconColor);
	}
}

void UModularButton::ApplyTextStyle()
{
	// As Common Bound Action Button does for its label. A button Style without text styles leaves the label as it is in the designer.
	UCommonTextBlock* CommonLabel = Cast<UCommonTextBlock>(Label);
	const TSubclassOf<UCommonTextStyle> TextStyle = GetCurrentTextStyleClass();
	if (CommonLabel && TextStyle)
	{
		CommonLabel->SetStyle(TextStyle);
	}
}

void UModularButton::ApplyJustification()
{
	const EHorizontalAlignment RowAlignment = Justification == EModularButtonJustify::Center ? HAlign_Center
		: Justification == EModularButtonJustify::Right ? HAlign_Right
		: HAlign_Left;
	if (Label)
	{
		Label->SetJustification(Justification == EModularButtonJustify::Center ? ETextJustify::Center
			: Justification == EModularButtonJustify::Right ? ETextJustify::Right
			: ETextJustify::Left);
	}

	// The Horizontal Box holding the icon and label moves as one: in an Overlay or a Border, or in the button itself when it's the root.
	const UWidget* Row = Label ? Label->GetParent() : Icon ? Icon->GetParent() : nullptr;
	UPanelSlot* RowSlot = Row ? Row->Slot.Get() : nullptr;
	if (UOverlaySlot* InOverlay = Cast<UOverlaySlot>(RowSlot))
	{
		InOverlay->SetHorizontalAlignment(RowAlignment);
	}
	else if (UBorderSlot* InBorder = Cast<UBorderSlot>(RowSlot))
	{
		InBorder->SetHorizontalAlignment(RowAlignment);
	}
	else if (UButtonSlot* InButton = Cast<UButtonSlot>(RowSlot))
	{
		InButton->SetHorizontalAlignment(RowAlignment);
	}
}
