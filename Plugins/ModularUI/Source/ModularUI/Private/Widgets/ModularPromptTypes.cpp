#include "Widgets/ModularPromptTypes.h"

#include "CommonButtonBase.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

#include "Widgets/ModularButton.h"

void FModularPromptView::Show(const FModularPromptRequest& InRequest)
{
	// Whatever the request says, there's a range to choose from.
	Request = InRequest;
	Request.MinCount = FMath::Max(0, Request.MinCount);
	Request.MaxCount = FMath::Max(Request.MinCount, Request.MaxCount);
	Request.DefaultCount = FMath::Clamp(Request.DefaultCount, Request.MinCount, Request.MaxCount);
	const bool bQuantity = Request.Kind == EModularPromptKind::Quantity;

	if (USlider* Slider = QuantitySlider.Get())
	{
		Slider->SetMinValue(static_cast<float>(Request.MinCount));
		Slider->SetMaxValue(static_cast<float>(Request.MaxCount));
		Slider->SetStepSize(1.0f);
		Slider->SetValue(static_cast<float>(Request.DefaultCount));
		Slider->SetVisibility(bQuantity ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (UTextBlock* Count = QuantityText.Get())
	{
		Count->SetVisibility(bQuantity ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UTextBlock* Title = TitleText.Get())
	{
		Title->SetText(Request.Title);
		Title->SetVisibility(Request.Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// Labels only where the request has one, so buttons keep their own otherwise.
	UModularButton* Confirm = Cast<UModularButton>(ConfirmButton.Get());
	if (Confirm && !Request.ConfirmLabel.IsEmpty())
	{
		Confirm->SetLabelText(Request.ConfirmLabel);
	}
	UModularButton* Cancel = Cast<UModularButton>(CancelButton.Get());
	if (Cancel && !Request.CancelLabel.IsEmpty())
	{
		Cancel->SetLabelText(Request.CancelLabel);
	}

	Refresh();
}

void FModularPromptView::Refresh() const
{
	const int32 Count = Request.Kind == EModularPromptKind::Quantity ? GetCount() : 0;
	if (UTextBlock* Message = MessageText.Get())
	{
		Message->SetText(FText::FormatNamed(Request.Message, TEXT("Count"), FText::AsNumber(Count), TEXT("Max"), FText::AsNumber(Request.MaxCount)));
	}
	if (UTextBlock* Quantity = QuantityText.Get())
	{
		Quantity->SetText(FText::FormatNamed(QuantityFormat, TEXT("Count"), FText::AsNumber(Count), TEXT("Max"), FText::AsNumber(Request.MaxCount)));
	}
}

int32 FModularPromptView::GetCount() const
{
	if (Request.Kind != EModularPromptKind::Quantity)
	{
		return 0;
	}
	const USlider* Slider = QuantitySlider.Get();
	return Slider ? FMath::Clamp(FMath::RoundToInt32(Slider->GetValue()), Request.MinCount, Request.MaxCount) : Request.DefaultCount;
}
