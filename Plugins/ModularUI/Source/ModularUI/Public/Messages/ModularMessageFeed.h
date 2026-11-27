#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/UserWidgetPool.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"

#include "Messages/ModularMessageEntryWidget.h"
#include "Messages/ModularMessageTypes.h"
#include "ModularUITransition.h"

#include "ModularMessageFeed.generated.h"

class UImage;
class UModularInputKeyWidget;
class UPanelWidget;
class UTextBlock;
class UWidgetAnimation;


/** How a feed shows the messages of its channels. */
UENUM(BlueprintType)
enum class EModularMessageFeedMode : uint8
{
	/** Several at once, each going by itself, such as toasts for items picked up. Past Max Shown, the oldest goes early. */
	Stack,
	/** One at a time, each once the last one has gone, so none is missed, such as new objectives. */
	Queue,
	/** One at a time, a new one taking the place of the one shown at once, such as hints, or what the player is looking at. */
	Replace
};


/**
 * A message a feed shows, or is sending away. A struct the garbage collector sees, so what the message refers to, such as its Payload, is
 * cleared rather than left dangling should it be destroyed while the message shows.
 */
USTRUCT()
struct FModularShownMessage
{
	GENERATED_BODY()

	int32 Id = 0;

	UPROPERTY()
	FModularMessage Message;

	TWeakObjectPtr<UModularMessageEntryWidget> Entry;
	FTSTicker::FDelegateHandle Expiry;
	bool bHiding = false;
};


/**
 * Shows the messages sent to its channels, such as toasts stacking up in a corner, a hint banner across the top of the screen, a large
 * heading for a new objective, or what the player is looking at near the bottom. Place one for each way your game shows messages, in the
 * HUD, or in the root layout to show over menus too, and give each its Channels.
 * With an Entry Class, each message is an entry made in Entry Panel. Without one, the feed shows one message at a time in its own named
 * widgets, Title Text, Message Text, Icon Image, Count Text and Key Widget, inside Message Panel.
 * Messages time out in real time, so they go while the game is paused too. One that stays until it's hidden, with a key and no time, such as
 * what the player is looking at, comes back after a newer message took its place and has gone, unless it was hidden meanwhile. With the log
 * category LogModularUI at Verbose, the log says as each message shows and goes, and why it goes.
 */
UCLASS(Abstract)
class MODULARUI_API UModularMessageFeed : public UUserWidget
{
	GENERATED_BODY()

public:
	UModularMessageFeed(const FObjectInitializer& ObjectInitializer);

	/** Whether it shows messages of the channel: one of Channels lists it, or a parent of it. Empty Channels shows every channel. */
	bool ShowsChannel(const FGameplayTag& Channel) const;

	// What the Modular Message Subsystem calls.
	void ReceiveMessage(const FModularMessage& Message);
	void ReceiveHide(const FGameplayTag& Channel, FName Key);
	void ReceiveClear(const FGameplayTag& Channel);

	/** Hides every message shown, and forgets those waiting. */
	UFUNCTION(BlueprintCallable, Category = "Message Feed")
	void ClearMessages();

	/** How many messages show now, those going included. */
	UFUNCTION(BlueprintPure, Category = "Message Feed")
	int32 GetNumShown() const { return Shown.Num(); }

	//~ Begin UWidget Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget Interface

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** A message started showing, or with Merged, a repeat merged into one showing, such as to play a sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Message Feed")
	void OnMessageShown(const FModularMessage& Message, bool bMerged);

	/** A message started to go. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Message Feed")
	void OnMessageHidden(const FModularMessage& Message);

	/** The channels it shows, and their children, such as UI.Message.Toast. Empty shows every channel. */
	UPROPERTY(EditAnywhere, Category = "Message Feed", meta = (Categories = "UI.Message"))
	FGameplayTagContainer Channels;

	/** Stack shows several at once, Queue one after another, and Replace one at a time, the newest at once. */
	UPROPERTY(EditAnywhere, Category = "Message Feed")
	EModularMessageFeedMode Mode = EModularMessageFeedMode::Replace;

	/**
	 * How long a message shows, in seconds, when it doesn't say: one with a Duration below 0. 0 shows it until it's hidden or replaced. A
	 * message's own Duration, such as 0 for what the player is looking at, always comes first.
	 */
	UPROPERTY(EditAnywhere, Category = "Message Feed", meta = (ClampMin = "0", Units = "s"))
	float DefaultDuration = 3.0f;

	/**
	 * A message the same as one showing joins it instead of showing again, such as a second herb picked up: its count adds up, and it shows
	 * for its full time again. A message with the key of one showing always takes its place.
	 */
	UPROPERTY(EditAnywhere, Category = "Message Feed")
	bool bMergeRepeats = true;

	/** The most shown at once. A new one past it sends the oldest of the lowest priority away early. */
	UPROPERTY(EditAnywhere, Category = "Message Feed", meta = (ClampMin = "1", EditCondition = "Mode == EModularMessageFeedMode::Stack", EditConditionHides))
	int32 MaxShown = 4;

	/** The most waiting at once. Past it, the newest of the lowest priority is dropped. */
	UPROPERTY(EditAnywhere, Category = "Message Feed", meta = (ClampMin = "0", EditCondition = "Mode == EModularMessageFeedMode::Queue", EditConditionHides))
	int32 MaxWaiting = 8;

	// Entries
	/** Made in Entry Panel for each message. None shows one message at a time in the feed's own widgets. */
	UPROPERTY(EditAnywhere, Category = "Message Feed|Entries")
	TSubclassOf<UModularMessageEntryWidget> EntryClass;

	/** Holds the entries, newest last, such as a Vertical Box. */
	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Entries", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EntryPanel;

	// Without an Entry Class
	/** Holds the feed's own message widgets, shown only while a message is, and moved by the transitions. Without it, the whole feed is. */
	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Single Message", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MessagePanel;

	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Single Message", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The message's Body. */
	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Single Message", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Single Message", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** The message's Count, in Count Format. Collapsed for 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Single Message", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** The key of the message's Input Action. */
	UPROPERTY(BlueprintReadOnly, Category = "Message Feed|Single Message", meta = (BindWidgetOptional))
	TObjectPtr<UModularInputKeyWidget> KeyWidget;

	/** What Count Text says. {Count} is the message's count. */
	UPROPERTY(EditAnywhere, Category = "Message Feed|Single Message")
	FText CountFormat = INVTEXT("x{Count}");

	/** How the message comes in, on Message Panel or the feed. */
	UPROPERTY(EditAnywhere, Category = "Message Feed|Single Message")
	FModularUITransition ShowTransition;

	/** The message goes by playing Show Transition backwards. Off, it plays Hide Transition. */
	UPROPERTY(EditAnywhere, Category = "Message Feed|Single Message")
	bool bHideReversesShow = false;

	UPROPERTY(EditAnywhere, Category = "Message Feed|Single Message", meta = (EditCondition = "!bHideReversesShow", EditConditionHides))
	FModularUITransition HideTransition;

	/** Played from the start as each message shows. Name an animation ShowAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

	/** Played as a message goes, which waits for it. Name an animation HideAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HideAnimation;

private:
	using FShownMessage = FModularShownMessage;

	bool UsesEntries() const;
	UWidget* GetSingleMessageWidget();

	FShownMessage* FindShown(int32 Id);
	FShownMessage* FindRepeat(const FModularMessage& Message);
	int32 CountShowing() const;

	/** How long the message shows here: its own Duration, or Default Duration. 0 until it's hidden. */
	float GetDuration(const FModularMessage& Message) const;

	/** Whether it stays until it's hidden by its key: it has a key, and shows here with no time. */
	bool IsLasting(const FModularMessage& Message) const;

	void Show(const FModularMessage& Message);
	void Merge(FShownMessage& Into, const FModularMessage& Repeat);

	/** Sends the message away, for Why, which the log says. */
	void Hide(int32 Id, const TCHAR* Why);
	void RemoveShown(int32 Id);
	void Wait(const FModularMessage& Message);

	/** Once one has gone: the next waiting in a queue, or else one a newer message took the place of, as there's room. */
	void ShowNextWaiting();
	void StartExpiry(FShownMessage& Message);

	/** A newer message is taking the place of one showing: one that stays until hidden comes back once there's room again. */
	void Displace(const FShownMessage& Message);

	/** Drops every message shown and waiting at once, without hiding them. Those to come back stay. */
	void Reset();

	UPROPERTY(Transient)
	TArray<FModularShownMessage> Shown;

	UPROPERTY(Transient)
	TArray<FModularMessage> Waiting;

	int32 NextId = 0;

	/** Messages that stay until hidden, which newer ones took the place of, the newest last, to come back once there's room. */
	UPROPERTY(Transient)
	TArray<FModularMessage> Displaced;

	UPROPERTY(Transient)
	FUserWidgetPool EntryPool;

	FModularMessageDisplay SingleDisplay;
	FModularMessageMotion SingleMotion;
};
