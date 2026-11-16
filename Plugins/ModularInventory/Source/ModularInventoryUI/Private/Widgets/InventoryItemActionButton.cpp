#include "Widgets/InventoryItemActionButton.h"

#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include "Actions/InventoryItemAction.h"

void UInventoryItemActionButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	AvailableStyle = GetStyle() ? GetStyle()->GetClass() : nullptr;
}

void UInventoryItemActionButton::SetAction(UInventoryItemAction* InAction, const FText& Label, bool bInAvailable, const FText& UnavailableReason)
{
	Action = InAction;
	bAvailable = bInAvailable;

	if (NameText)
	{
		NameText->SetText(Label);
	}
	if (Icon)
	{
		UTexture2D* Texture = Action ? Action->Icon.Get() : nullptr;
		if (Texture)
		{
			Icon->SetBrushFromTexture(Texture);
		}
		Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ReasonText)
	{
		const bool bShowReason = !bAvailable && !UnavailableReason.IsEmpty();
		ReasonText->SetText(bShowReason ? UnavailableReason : FText::GetEmpty());
		ReasonText->SetVisibility(bShowReason ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const TSubclassOf<UCommonButtonStyle> Wanted = !bAvailable && UnavailableStyle ? UnavailableStyle : AvailableStyle;
	const UCommonButtonStyle* Current = GetStyle();
	if (Wanted && (!Current || Current->GetClass() != Wanted))
	{
		SetStyle(Wanted);
	}
	OnActionChanged(bAvailable);
}
