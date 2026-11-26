#include "Messages/ModularMessageSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "Messages/ModularMessageFeed.h"
#include "ModularUI.h"

UModularMessageSubsystem* UModularMessageSubsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UModularMessageSubsystem>() : nullptr;
}

UModularMessageSubsystem* UModularMessageSubsystem::GetForPlayer(const APlayerController* PlayerController)
{
	return PlayerController ? Get(PlayerController->GetLocalPlayer()) : nullptr;
}

void UModularMessageSubsystem::ShowMessage(const FModularMessage& Message)
{
	if (!Message.Channel.IsValid())
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] A message without a channel can't be shown: \"%s\"."), *Message.Body.ToString());
		return;
	}

	// One that stays until hidden is kept for feeds made later, in place of any with its key.
	if (!Message.Key.IsNone())
	{
		Lasting.RemoveAll([&Message](const FModularMessage& Kept) { return Kept.Key == Message.Key && Kept.Channel == Message.Channel; });
		if (Message.Duration == 0.0f)
		{
			Lasting.Add(Message);
		}
	}

	// A copy, as a feed's Blueprint can show messages of its own as it shows this one.
	Feeds.RemoveAll([](const TWeakObjectPtr<UModularMessageFeed>& Feed) { return !Feed.IsValid(); });
	const TArray<TWeakObjectPtr<UModularMessageFeed>> Listening = Feeds;
	bool bShown = false;
	for (const TWeakObjectPtr<UModularMessageFeed>& WeakFeed : Listening)
	{
		UModularMessageFeed* Feed = WeakFeed.Get();
		if (Feed && Feed->ShowsChannel(Message.Channel))
		{
			Feed->ReceiveMessage(Message);
			bShown = true;
		}
	}
	UE_CLOG(!bShown, LogModularUI, Verbose, TEXT("[Modular UI] No feed shows the channel %s, so \"%s\" isn't shown."), *Message.Channel.ToString(), *Message.Body.ToString());

	OnMessageShown.Broadcast(Message);
}

void UModularMessageSubsystem::ShowText(FGameplayTag Channel, const FText& Text, float Duration)
{
	FModularMessage Message;
	Message.Channel = Channel;
	Message.Body = Text;
	Message.Duration = Duration;
	ShowMessage(Message);
}

void UModularMessageSubsystem::HideMessage(FGameplayTag Channel, FName Key)
{
	if (Key.IsNone())
	{
		return;
	}

	Lasting.RemoveAll([Channel, Key](const FModularMessage& Kept) { return Kept.Key == Key && Kept.Channel == Channel; });
	const TArray<TWeakObjectPtr<UModularMessageFeed>> Listening = Feeds;
	for (const TWeakObjectPtr<UModularMessageFeed>& WeakFeed : Listening)
	{
		UModularMessageFeed* Feed = WeakFeed.Get();
		if (Feed && Feed->ShowsChannel(Channel))
		{
			Feed->ReceiveHide(Channel, Key);
		}
	}
	OnMessageHidden.Broadcast(Channel, Key);
}

void UModularMessageSubsystem::ClearChannel(FGameplayTag Channel)
{
	Lasting.RemoveAll([Channel](const FModularMessage& Kept) { return Kept.Channel.MatchesTag(Channel); });
	const TArray<TWeakObjectPtr<UModularMessageFeed>> Listening = Feeds;
	for (const TWeakObjectPtr<UModularMessageFeed>& WeakFeed : Listening)
	{
		if (UModularMessageFeed* Feed = WeakFeed.Get())
		{
			Feed->ReceiveClear(Channel);
		}
	}
}

void UModularMessageSubsystem::RegisterFeed(UModularMessageFeed& Feed)
{
	if (Feeds.Contains(&Feed))
	{
		return;
	}
	Feeds.Add(&Feed);

	// What's already showing until hidden, such as what the player looks at as the HUD is built.
	for (const FModularMessage& Kept : Lasting)
	{
		if (Feed.ShowsChannel(Kept.Channel))
		{
			Feed.ReceiveMessage(Kept);
		}
	}
}

void UModularMessageSubsystem::UnregisterFeed(UModularMessageFeed& Feed)
{
	Feeds.RemoveAll([&Feed](const TWeakObjectPtr<UModularMessageFeed>& Registered) { return !Registered.IsValid() || Registered.Get() == &Feed; });
}
