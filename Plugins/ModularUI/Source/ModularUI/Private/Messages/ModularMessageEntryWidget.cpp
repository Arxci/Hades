#include "Messages/ModularMessageEntryWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

#include "Widgets/ModularInputKeyWidget.h"

UModularMessageEntryWidget::UModularMessageEntryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Comes up a little as it fades in, and fades out where it is.
	ShowTransition.Duration = 0.15f;
	ShowTransition.SlideDistance = 12.0f;
	ShowTransition.SlideFrom = EModularUISlide::FromBelow;
	HideTransition.Duration = 0.25f;
	HideTransition.bSlide = false;
}

void UModularMessageEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Display.TitleText = TitleText;
	Display.MessageText = MessageText;
	Display.IconImage = IconImage;
	Display.CountText = CountText;
	Display.KeyWidget = KeyWidget;
	Display.CountFormat = CountFormat;
	Motion.Init(*this, *this, ShowAnimation, HideAnimation);
}

void UModularMessageEntryWidget::NativeDestruct()
{
	Motion.Stop();
	Display.CancelLoad();

	Super::NativeDestruct();
}

void UModularMessageEntryWidget::SetMessage(const FModularMessage& InMessage, bool bMerged)
{
	Message = InMessage;
	Display.Show(Message);
	OnMessageChanged(Message, bMerged);
}

void UModularMessageEntryWidget::PlayShow()
{
	Motion.PlayShow(ShowTransition);
}

void UModularMessageEntryWidget::PlayHide(TFunction<void()>&& OnHidden)
{
	Motion.PlayHide(bHideReversesShow, HideTransition, MoveTemp(OnHidden));
}
