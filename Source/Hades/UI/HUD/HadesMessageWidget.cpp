#include "HadesMessageWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"

#include "Hades/Player/HadesPlayerController.h"

void UHadesMessageWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MessageText->SetText(FText::GetEmpty());
	if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer()))
	{
		Controller->OnMessage.AddUniqueDynamic(this, &ThisClass::HandleMessage);
	}
}

void UHadesMessageWidget::NativeDestruct()
{
	if (AHadesPlayerController* Controller = Cast<AHadesPlayerController>(GetOwningPlayer()))
	{
		Controller->OnMessage.RemoveDynamic(this, &ThisClass::HandleMessage);
	}

	Super::NativeDestruct();
}

void UHadesMessageWidget::HandleMessage(const FText& Message)
{
	MessageText->SetText(Message);
	if (ShowAnimation)
	{
		PlayAnimation(ShowAnimation);
	}
}
