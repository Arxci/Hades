#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "Messages/ModularMessageTypes.h"

#include "ModularMessageSubsystem.generated.h"

class APlayerController;
class UModularMessageFeed;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FModularMessageShownSignature, const FModularMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularMessageHiddenSignature, FGameplayTag, Channel, FName, Key);


/**
 * A local player's messages: anything sends one to a channel, such as a hint to UI.Message.Hint or a pickup to UI.Message.Toast, and every
 * Modular Message Feed showing that channel shows it, in its own way. Senders never need to know what shows their messages, so one game
 * can show hints as a banner and another as a toast, with the same code.
 * A message with a key and a Duration of 0 stays until it's hidden, such as what the player is looking at; one sent again with the same key
 * replaces it. Feeds made after it was sent, such as a HUD built later, still show it.
 */
UCLASS()
class MODULARUI_API UModularMessageSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UModularMessageSubsystem* Get(const ULocalPlayer* LocalPlayer);

	/** The subsystem of the local player the player controller belongs to. */
	static UModularMessageSubsystem* GetForPlayer(const APlayerController* PlayerController);

	/** Shows the message in every feed showing its channel. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Messages")
	void ShowMessage(const FModularMessage& Message);

	/** Shows a line of text on a channel, such as "It's locked." on UI.Message.Hint. Duration below 0 uses each feed's. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Messages")
	void ShowText(UPARAM(meta = (Categories = "UI.Message")) FGameplayTag Channel, const FText& Text, float Duration = -1.0f);

	/** Hides the message with the key on the channel, wherever it shows, and forgets it if it was waiting. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Messages")
	void HideMessage(UPARAM(meta = (Categories = "UI.Message")) FGameplayTag Channel, FName Key);

	/** Hides every message on the channel, and its children, and forgets those waiting. */
	UFUNCTION(BlueprintCallable, Category = "Modular UI|Messages")
	void ClearChannel(UPARAM(meta = (Categories = "UI.Message")) FGameplayTag Channel);

	/** Feeds register as they're made, to hear the channels they show, and get the messages already showing until hidden. */
	void RegisterFeed(UModularMessageFeed& Feed);
	void UnregisterFeed(UModularMessageFeed& Feed);

	/** A message was sent, whether or not a feed shows it, such as to play a sound for it. */
	UPROPERTY(BlueprintAssignable, Category = "Modular UI|Messages")
	FModularMessageShownSignature OnMessageShown;

	/** Hide Message was called for a key. */
	UPROPERTY(BlueprintAssignable, Category = "Modular UI|Messages")
	FModularMessageHiddenSignature OnMessageHidden;

private:
	TArray<TWeakObjectPtr<UModularMessageFeed>> Feeds;

	/**
	 * Keyed messages that stay until hidden, for feeds made after they were sent. Kept as a property, so what a message refers to, such as
	 * its Payload, is known to the garbage collector, which clears it should it be destroyed meanwhile.
	 */
	UPROPERTY(Transient)
	TArray<FModularMessage> Lasting;
};
