#include "Actions/AsyncAction_ShowModularPrompt.h"

#include "GameFramework/PlayerController.h"

#include "Layers/ModularUISubsystem.h"
#include "Widgets/ModularPromptWidget.h"

UAsyncAction_ShowModularPrompt* UAsyncAction_ShowModularPrompt::ShowModularPrompt(APlayerController* PlayerController, TSubclassOf<UModularPromptWidget> PromptClass,
	FGameplayTag LayerTag, const FModularPromptRequest& Request)
{
	UAsyncAction_ShowModularPrompt* Action = NewObject<UAsyncAction_ShowModularPrompt>();
	Action->PlayerController = PlayerController;
	Action->PromptClass = PromptClass;
	Action->LayerTag = LayerTag;
	Action->Request = Request;
	Action->RegisterWithGameInstance(PlayerController);
	return Action;
}

void UAsyncAction_ShowModularPrompt::Activate()
{
	UModularUISubsystem* UI = UModularUISubsystem::GetForPlayer(PlayerController.Get());
	const FModularPromptRequest& Asked = Request;
	UModularPromptWidget* Prompt = UI && PromptClass ? UI->PushWidgetToLayer<UModularPromptWidget>(LayerTag, PromptClass.Get(), [&Asked](UModularPromptWidget& Widget)
	{
		Widget.SetRequest(Asked);
	}) : nullptr;

	if (!Prompt)
	{
		HandleAnswered(false, 0);
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis = this;
	Prompt->SetOnAnswered([WeakThis](bool bConfirmed, int32 Count)
	{
		if (ThisClass* Action = WeakThis.Get())
		{
			Action->HandleAnswered(bConfirmed, Count);
		}
	});
}

void UAsyncAction_ShowModularPrompt::HandleAnswered(bool bConfirmed, int32 Count)
{
	if (bConfirmed)
	{
		Confirmed.Broadcast(Count);
	}
	else
	{
		Cancelled.Broadcast(Count);
	}
	SetReadyToDestroy();
}
