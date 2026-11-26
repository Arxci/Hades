#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"

#include "ModularUITransition.h"

#include "ModularMessageTypes.generated.h"

class UImage;
class UInputAction;
class UModularInputKeyWidget;
class UTextBlock;
class UTexture2D;
class UUserWidget;
class UWidget;
class UWidgetAnimation;


/**
 * Something to tell the player, sent to a channel: the feeds showing that channel show it, each in its own way, such as a stack of toasts,
 * a banner across the screen or a large heading. Anything can send one with the Modular Message Subsystem, without knowing what shows it.
 */
USTRUCT(BlueprintType)
struct MODULARUI_API FModularMessage
{
	GENERATED_BODY()

	/** Which feeds show it: those whose Channels list this tag, or a parent of it, such as UI.Message.Toast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message", meta = (Categories = "UI.Message"))
	FGameplayTag Channel;

	/**
	 * Tells the message apart, so it can be changed or hidden later, such as what the player is looking at, and so a message with the key of
	 * one shown replaces it. None for a one-off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FName Key;

	/** Such as an item's name, or a new objective. Its widget collapses while it's empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText Title;

	/** The message itself, such as "It's locked." Its widget collapses while it's empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message", meta = (MultiLine = true))
	FText Body;

	/** Such as the icon of an item picked up. Loaded as it's shown, and collapsed without one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	TSoftObjectPtr<UTexture2D> Icon;

	/** A count shown with it, such as how many were picked up. 0 shows none. Repeats a feed merges add theirs up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message", meta = (ClampMin = "0"))
	int32 Count = 0;

	/** An Enhanced Input action whose key it shows, such as the key that picks an item up, in a Key Widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	TObjectPtr<const UInputAction> InputAction;

	/** How long it shows, in seconds, in real time so it counts down while the game is paused. Below 0 uses the feed's; 0 shows it until it's hidden or replaced. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	float Duration = -1.0f;

	/** Higher comes first in a feed that queues, and stays longest in a full one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	int32 Priority = 0;

	/** What kind of message it is within its channel, for entries to look different, such as a tag for warnings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FGameplayTag Style;

	/** Anything else an entry shows, such as an item definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	TObjectPtr<UObject> Payload;

	/** Whether it's the same message as Other, for a feed to merge repeats: the same key, or without keys, the same channel, text and icon. */
	bool IsRepeatOf(const FModularMessage& Other) const;
};


/** The named widgets a message shows in, such as a feed's entry or a feed that shows its one message itself. */
struct MODULARUI_API FModularMessageDisplay
{
	TWeakObjectPtr<UTextBlock> TitleText;
	TWeakObjectPtr<UTextBlock> MessageText;
	TWeakObjectPtr<UTextBlock> CountText;
	TWeakObjectPtr<UImage> IconImage;
	TWeakObjectPtr<UModularInputKeyWidget> KeyWidget;

	/** What Count Text says. {Count} is the message's count. */
	FText CountFormat = INVTEXT("x{Count}");

	~FModularMessageDisplay();

	/** Fills the widgets in, collapsing those the message has nothing for. The icon loads first if it has to, hidden meanwhile. */
	void Show(const FModularMessage& Message);
	void CancelLoad();

private:
	TSharedPtr<FStreamableHandle> IconHandle;
};


/**
 * Brings a message's widget in and out: Show Transition and the widget's Show Animation as it shows, and Hide Transition, or the show
 * reversed, and Hide Animation as it goes. Played from the core ticker, so it plays while the game is paused.
 */
struct MODULARUI_API FModularMessageMotion
{
	~FModularMessageMotion();

	/** Owner plays the animations, and Target fades, slides and grows, such as the entry itself. */
	void Init(UUserWidget& InOwner, UWidget& InTarget, UWidgetAnimation* InShowAnimation, UWidgetAnimation* InHideAnimation);

	void PlayShow(const FModularUITransition& ShowTransition);

	/** Goes, then calls On Hidden. With Hide Reverses Show, the show goes back from wherever it got to; otherwise Hide Transition plays. */
	void PlayHide(bool bHideReversesShow, const FModularUITransition& HideTransition, TFunction<void()>&& OnHidden);

	/** Stops where it is, without calling back. */
	void Stop();

	bool IsHiding() const { return bHiding; }

private:
	void StartTicker();

	TWeakObjectPtr<UUserWidget> Owner;
	TWeakObjectPtr<UWidgetAnimation> ShowAnimation;
	TWeakObjectPtr<UWidgetAnimation> HideAnimation;
	TWeakObjectPtr<UWidgetAnimation> AwaitedAnimation;
	FModularUITransitionPlayer Player;
	TFunction<void()> AfterHidden;
	FTSTicker::FDelegateHandle TickHandle;
	bool bHiding = false;
};
