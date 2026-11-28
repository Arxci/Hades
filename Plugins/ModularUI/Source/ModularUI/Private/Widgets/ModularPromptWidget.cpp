#include "Widgets/ModularPromptWidget.h"

#include "CommonButtonBase.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

#include "ModularUISettings.h"

UModularPromptWidget::UModularPromptWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsModal = true;
	InputConfig = EModularUIInputMode::Menu;
}

void UModularPromptWidget::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Dialogs saved before Right Click replaced Right Click Cancels. Only a dialog that turned it off saved it, so only that's carried over.
	if (!bRightClickCancels_DEPRECATED)
	{
		RightClick = EModularPromptRightClick::Ignore;
		bRightClickCancels_DEPRECATED = true;
	}
#endif
}

bool UModularPromptWidget::DoesRightClickCancel() const
{
	switch (RightClick)
	{
	case EModularPromptRightClick::Cancel:
		return true;
	case EModularPromptRightClick::Ignore:
		return false;
	case EModularPromptRightClick::ProjectDefault:
		break;
	}
	return GetDefault<UModularUISettings>()->ExtraBackKeys.Contains(EKeys::RightMouseButton);
}

void UModularPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	View.TitleText = TitleText;
	View.MessageText = PromptText;
	View.ConfirmButton = ConfirmButton;
	View.CancelButton = CancelButton;
	View.QuantitySlider = QuantitySlider;
	View.QuantityText = QuantityText;
	View.QuantityFormat = QuantityFormat;

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::Confirm);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::Cancel);
	}
	if (QuantitySlider)
	{
		QuantitySlider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleQuantityChanged);
	}
}

void UModularPromptWidget::SetRequest(const FModularPromptRequest& InRequest)
{
	View.Show(InRequest);
	OnRequestChanged(View.GetRequest());
}

void UModularPromptWidget::SetOnAnswered(TFunction<void(bool bConfirmed, int32 Count)>&& InOnAnswered)
{
	OnAnsweredNative = MoveTemp(InOnAnswered);
}

FModularPromptRequest UModularPromptWidget::GetRequest() const
{
	return View.GetRequest();
}

int32 UModularPromptWidget::GetChosenCount() const
{
	return View.GetCount();
}

void UModularPromptWidget::HandleQuantityChanged(float Value)
{
	View.Refresh();
}

void UModularPromptWidget::NativeOnActivated()
{
	// Dialogs are reused, so each opening waits for an answer of its own.
	bAnswered = false;

	Super::NativeOnActivated();
}

void UModularPromptWidget::NativeOnDeactivated()
{
	// Closed without an answer, such as by its layer being cleared: that's a no.
	if (!bAnswered)
	{
		Answer(false, /*bClose*/ false);
	}
	OnAnsweredNative = TFunction<void(bool, int32)>();

	Super::NativeOnDeactivated();
}

UWidget* UModularPromptWidget::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Target = Super::NativeGetDesiredFocusTarget())
	{
		return Target;
	}

	// The count to choose, then the safe answer to a confirmation, or else the yes.
	if (View.GetRequest().Kind == EModularPromptKind::Quantity && QuantitySlider && QuantitySlider->IsVisible())
	{
		return QuantitySlider.Get();
	}
	if (bConfirmStartsOnCancel && CancelButton && CancelButton->IsVisible())
	{
		return CancelButton.Get();
	}
	return ConfirmButton ? static_cast<UWidget*>(ConfirmButton.Get()) : static_cast<UWidget*>(CancelButton.Get());
}

bool UModularPromptWidget::NativeOnHandleBackAction()
{
	if (!IsClosing() && !BP_OnHandleBackAction())
	{
		Cancel();
	}
	return true;
}

FReply UModularPromptWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsClosing())
	{
		return FReply::Handled();
	}

	const FKey Button = InMouseEvent.GetEffectingButton();
	if (Button == EKeys::RightMouseButton)
	{
		if (DoesRightClickCancel())
		{
			Cancel();
		}
		return FReply::Handled();
	}

	const bool bOnPanel = PromptPanel && PromptPanel->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	if (!bOnPanel && PromptPanel && bClickOutsideCancels)
	{
		Cancel();
	}

	// Presses on the dialog reach its buttons first, so whatever comes here goes no further.
	return FReply::Handled();
}

FReply UModularPromptWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The second of two quick presses comes as a double-click: a press like the first.
	return NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UModularPromptWidget::Confirm()
{
	Answer(true, /*bClose*/ true);
}

void UModularPromptWidget::Cancel()
{
	Answer(false, /*bClose*/ true);
}

void UModularPromptWidget::Answer(bool bConfirmed, bool bClose)
{
	if (bAnswered)
	{
		return;
	}

	// Answered once: the callback is taken first, as closing can reuse the dialog for another question.
	bAnswered = true;
	const int32 Count = bConfirmed ? View.GetCount() : 0;
	TFunction<void(bool, int32)> Callback = MoveTemp(OnAnsweredNative);
	OnAnsweredNative = TFunction<void(bool, int32)>();
	if (bClose && !IsClosing())
	{
		CloseWidget();
	}

	if (Callback)
	{
		Callback(bConfirmed, Count);
	}
	OnAnswered.Broadcast(bConfirmed, Count);
}
