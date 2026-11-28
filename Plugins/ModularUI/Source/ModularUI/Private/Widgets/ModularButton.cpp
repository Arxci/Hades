#include "Widgets/ModularButton.h"

#include "CommonTextBlock.h"
#include "Components/BorderSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include "ModularActivatableWidget.h"
#include "ModularUIFocus.h"
#include "Widgets/ModularInputKeyWidget.h"

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
	RefreshKeyWidget();
}

void UModularButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();

	RefreshKeyWidget();
}

void UModularButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();

	RefreshKeyWidget();
}

void UModularButton::UpdateInputActionWidget()
{
	// Common UI updates its own Input Action Widget here, such as when the button's triggering action changes.
	Super::UpdateInputActionWidget();

	RefreshKeyWidget();
}

void UModularButton::RefreshKeyWidget()
{
	if (!KeyWidget)
	{
		return;
	}

	// Only the button's own key, never Accept's: none without a triggering action, and none while the button can't be pressed, unless
	// Common UI's Display Input Action When Not Interactable says to show it then too.
	const bool bPressable = GetIsEnabled() && IsInteractionEnabled();
	const UInputAction* ShownAction = bPressable || bDisplayInputActionWhenNotInteractable ? GetEnhancedInputAction() : nullptr;
	if (KeyWidget->GetInputAction() != ShownAction)
	{
		KeyWidget->SetInputAction(ShownAction);
	}
}

void UModularButton::NativeOnHovered()
{
	Super::NativeOnHovered();

	SetHoverLit(true);
	SetHighlightedForDescription(true);
}

void UModularButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	SetHoverLit(false);
	SetHighlightedForDescription(false);
}

void UModularButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	SetHighlightedForDescription(true);
}

void UModularButton::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	SetHighlightedForDescription(false);
}

void UModularButton::SetDescription(const FText& InDescription)
{
	Description = InDescription;
	if (IsHovered() || HasAnyUserFocus())
	{
		SetHighlightedForDescription(true);
	}
}

void UModularButton::SetHighlightedForDescription(bool bHighlighted)
{
	// Buttons without a description leave the screen's Description Text as it is, such as the last option's while the mouse crosses them.
	UModularActivatableWidget* Screen = !IsDesignTime() ? ModularUI::FindDescriptionOwner(*this) : nullptr;
	if (!Screen)
	{
		return;
	}
	if (!bHighlighted)
	{
		Screen->UnhighlightForDescription(this);
	}
	else if (!Description.IsEmpty())
	{
		Screen->HighlightForDescription(this);
	}
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
	// The designer's layout stands.
	if (Justification == EModularButtonJustify::AsDesigned)
	{
		return;
	}

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
