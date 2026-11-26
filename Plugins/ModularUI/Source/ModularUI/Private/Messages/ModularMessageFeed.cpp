#include "Messages/ModularMessageFeed.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

#include "Messages/ModularMessageEntryWidget.h"
#include "Messages/ModularMessageSubsystem.h"
#include "ModularUI.h"
#include "Widgets/ModularInputKeyWidget.h"

UModularMessageFeed::UModularMessageFeed(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EntryPool(*this)
{
	ShowTransition.Duration = 0.15f;
	ShowTransition.SlideDistance = 12.0f;
	ShowTransition.SlideFrom = EModularUISlide::FromBelow;
	HideTransition.Duration = 0.25f;
	HideTransition.bSlide = false;
}

bool UModularMessageFeed::UsesEntries() const
{
	return EntryClass && EntryPanel;
}

bool UModularMessageFeed::ShowsChannel(const FGameplayTag& Channel) const
{
	return Channels.IsEmpty() || Channel.MatchesAny(Channels);
}

void UModularMessageFeed::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SingleDisplay.TitleText = TitleText;
	SingleDisplay.MessageText = MessageText;
	SingleDisplay.IconImage = IconImage;
	SingleDisplay.CountText = CountText;
	SingleDisplay.KeyWidget = KeyWidget;
	SingleDisplay.CountFormat = CountFormat;

	if (EntryClass && !EntryPanel)
	{
		UE_LOG(LogModularUI, Warning, TEXT("[Modular UI] %s has an Entry Class but no Entry Panel to put entries in, so it shows one message at a time itself."), *GetPathNameSafe(this));
	}

	// Without entries, the feed's own message shows only while there's one.
	if (!UsesEntries())
	{
		UWidget* Single = GetSingleMessageWidget();
		SingleMotion.Init(*this, *Single, ShowAnimation, HideAnimation);
		Single->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UWidget* UModularMessageFeed::GetSingleMessageWidget()
{
	return MessagePanel ? MessagePanel.Get() : this;
}

void UModularMessageFeed::NativeConstruct()
{
	Super::NativeConstruct();

	if (UModularMessageSubsystem* Messages = UModularMessageSubsystem::Get(GetOwningLocalPlayer()))
	{
		Messages->RegisterFeed(*this);
	}
}

void UModularMessageFeed::NativeDestruct()
{
	if (UModularMessageSubsystem* Messages = UModularMessageSubsystem::Get(GetOwningLocalPlayer()))
	{
		Messages->UnregisterFeed(*this);
	}
	Reset();

	Super::NativeDestruct();
}

void UModularMessageFeed::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	EntryPool.ResetPool();
}

void UModularMessageFeed::Reset()
{
	for (FShownMessage& Message : Shown)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Message.Expiry);
		if (UModularMessageEntryWidget* Entry = Message.Entry.Get())
		{
			Entry->RemoveFromParent();
			EntryPool.Release(Entry);
		}
	}
	Shown.Reset();
	Waiting.Reset();
	SingleMotion.Stop();
	SingleDisplay.CancelLoad();
	if (!UsesEntries())
	{
		GetSingleMessageWidget()->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UModularMessageFeed::ReceiveMessage(const FModularMessage& Message)
{
	// The same message again, or a new one with the key of one showing, changes the one showing.
	if (FShownMessage* Repeat = FindRepeat(Message))
	{
		if (bMergeRepeats || !Message.Key.IsNone())
		{
			Merge(*Repeat, Message);
			return;
		}
	}

	// Queued, it waits until the one showing has gone.
	if (Mode == EModularMessageFeedMode::Queue && !Shown.IsEmpty())
	{
		Wait(Message);
		return;
	}

	// One at a time, the newest at once: the one showing goes. Without entries, there's one place to show it, so the one there, showing or
	// going, makes way at once.
	if (!UsesEntries())
	{
		if (!Shown.IsEmpty())
		{
			Reset();
		}
	}
	else if (Mode == EModularMessageFeedMode::Replace)
	{
		TArray<int32> Showing;
		for (const FShownMessage& Other : Shown)
		{
			Showing.Add(Other.Id);
		}
		for (const int32 Id : Showing)
		{
			Hide(Id);
		}
	}

	Show(Message);

	// Too many at once: the oldest of the lowest priority goes early.
	while (Mode == EModularMessageFeedMode::Stack && UsesEntries() && CountShowing() > FMath::Max(1, MaxShown))
	{
		const FShownMessage* Oldest = nullptr;
		for (const FShownMessage& Other : Shown)
		{
			if (!Other.bHiding && (!Oldest || Other.Message.Priority < Oldest->Message.Priority))
			{
				Oldest = &Other;
			}
		}
		if (!Oldest)
		{
			break;
		}
		Hide(Oldest->Id);
	}
}

void UModularMessageFeed::ReceiveHide(const FGameplayTag& Channel, FName Key)
{
	Waiting.RemoveAll([&Channel, Key](const FModularMessage& Message) { return Message.Key == Key && Message.Channel == Channel; });

	TArray<int32> Matching;
	for (const FShownMessage& Message : Shown)
	{
		if (Message.Message.Key == Key && Message.Message.Channel == Channel)
		{
			Matching.Add(Message.Id);
		}
	}
	for (const int32 Id : Matching)
	{
		Hide(Id);
	}
}

void UModularMessageFeed::ReceiveClear(const FGameplayTag& Channel)
{
	Waiting.RemoveAll([&Channel](const FModularMessage& Message) { return Message.Channel.MatchesTag(Channel); });

	TArray<int32> Matching;
	for (const FShownMessage& Message : Shown)
	{
		if (Message.Message.Channel.MatchesTag(Channel))
		{
			Matching.Add(Message.Id);
		}
	}
	for (const int32 Id : Matching)
	{
		Hide(Id);
	}
}

void UModularMessageFeed::ClearMessages()
{
	Waiting.Reset();
	TArray<int32> All;
	for (const FShownMessage& Message : Shown)
	{
		All.Add(Message.Id);
	}
	for (const int32 Id : All)
	{
		Hide(Id);
	}
}

UModularMessageFeed::FShownMessage* UModularMessageFeed::FindShown(int32 Id)
{
	return Shown.FindByPredicate([Id](const FShownMessage& Message) { return Message.Id == Id; });
}

UModularMessageFeed::FShownMessage* UModularMessageFeed::FindRepeat(const FModularMessage& Message)
{
	return Shown.FindByPredicate([&Message](const FShownMessage& Other) { return !Other.bHiding && Message.IsRepeatOf(Other.Message); });
}

int32 UModularMessageFeed::CountShowing() const
{
	int32 Count = 0;
	for (const FShownMessage& Message : Shown)
	{
		Count += Message.bHiding ? 0 : 1;
	}
	return Count;
}

void UModularMessageFeed::Show(const FModularMessage& Message)
{
	const int32 Id = ++NextId;
	if (UsesEntries())
	{
		UModularMessageEntryWidget* Entry = EntryPool.GetOrCreateInstance<UModularMessageEntryWidget>(EntryClass);
		if (!Entry)
		{
			return;
		}
		EntryPanel->AddChild(Entry);
		Entry->SetVisibility(ESlateVisibility::HitTestInvisible);
		Entry->SetMessage(Message, /*bMerged*/ false);
		Entry->PlayShow();

		FShownMessage& New = Shown.AddDefaulted_GetRef();
		New.Id = Id;
		New.Message = Message;
		New.Entry = Entry;
	}
	else
	{
		SingleDisplay.Show(Message);
		GetSingleMessageWidget()->SetVisibility(ESlateVisibility::HitTestInvisible);
		SingleMotion.PlayShow(ShowTransition);

		FShownMessage& New = Shown.AddDefaulted_GetRef();
		New.Id = Id;
		New.Message = Message;
	}

	StartExpiry(Shown.Last());
	OnMessageShown(Message, /*bMerged*/ false);
}

void UModularMessageFeed::Merge(FShownMessage& Into, const FModularMessage& Repeat)
{
	// Counts add up for repeats; a keyed message otherwise takes the place of the one showing.
	FModularMessage Merged = Repeat;
	if (bMergeRepeats && Into.Message.Count > 0 && Repeat.Count > 0)
	{
		Merged.Count = Into.Message.Count + Repeat.Count;
	}
	Into.Message = Merged;

	if (UModularMessageEntryWidget* Entry = Into.Entry.Get())
	{
		Entry->SetMessage(Merged, /*bMerged*/ true);
	}
	else if (!UsesEntries())
	{
		SingleDisplay.Show(Merged);
	}
	StartExpiry(Into);
	OnMessageShown(Merged, /*bMerged*/ true);
}

void UModularMessageFeed::StartExpiry(FShownMessage& Message)
{
	FTSTicker::GetCoreTicker().RemoveTicker(Message.Expiry);
	Message.Expiry = FTSTicker::FDelegateHandle();

	const float Duration = Message.Message.Duration < 0.0f ? DefaultDuration : Message.Message.Duration;
	if (Duration <= 0.0f)
	{
		return;
	}

	// In real time, so it goes while the game is paused too.
	const int32 Id = Message.Id;
	Message.Expiry = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Id](float)
	{
		if (FShownMessage* Expired = FindShown(Id))
		{
			Expired->Expiry = FTSTicker::FDelegateHandle();
		}
		Hide(Id);
		return false;
	}), Duration);
}

void UModularMessageFeed::Hide(int32 Id)
{
	FShownMessage* Message = FindShown(Id);
	if (!Message || Message->bHiding)
	{
		return;
	}

	Message->bHiding = true;
	FTSTicker::GetCoreTicker().RemoveTicker(Message->Expiry);
	Message->Expiry = FTSTicker::FDelegateHandle();
	const FModularMessage Hidden = Message->Message;
	UModularMessageEntryWidget* Entry = Message->Entry.Get();

	const TWeakObjectPtr<ThisClass> WeakThis = this;
	TFunction<void()> OnHidden = [WeakThis, Id]()
	{
		if (ThisClass* Feed = WeakThis.Get())
		{
			Feed->RemoveShown(Id);
			Feed->ShowNextWaiting();
		}
	};

	if (Entry)
	{
		Entry->PlayHide(MoveTemp(OnHidden));
	}
	else if (!UsesEntries())
	{
		SingleMotion.PlayHide(bHideReversesShow, HideTransition, MoveTemp(OnHidden));
	}
	else
	{
		OnHidden();
	}
	OnMessageHidden(Hidden);
}

void UModularMessageFeed::RemoveShown(int32 Id)
{
	const int32 Index = Shown.IndexOfByPredicate([Id](const FShownMessage& Message) { return Message.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	FTSTicker::GetCoreTicker().RemoveTicker(Shown[Index].Expiry);
	if (UModularMessageEntryWidget* Entry = Shown[Index].Entry.Get())
	{
		Entry->RemoveFromParent();
		EntryPool.Release(Entry);
	}
	Shown.RemoveAt(Index);

	if (!UsesEntries() && Shown.IsEmpty())
	{
		GetSingleMessageWidget()->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UModularMessageFeed::Wait(const FModularMessage& Message)
{
	// A keyed one takes the place of one waiting with its key. Higher priorities wait ahead of lower ones, in the order they came.
	if (!Message.Key.IsNone())
	{
		Waiting.RemoveAll([&Message](const FModularMessage& Other) { return Other.Key == Message.Key && Other.Channel == Message.Channel; });
	}
	const int32 Before = Waiting.IndexOfByPredicate([&Message](const FModularMessage& Other) { return Other.Priority < Message.Priority; });
	Waiting.Insert(Message, Before == INDEX_NONE ? Waiting.Num() : Before);

	while (Waiting.Num() > FMath::Max(0, MaxWaiting))
	{
		Waiting.Pop();
	}
}

void UModularMessageFeed::ShowNextWaiting()
{
	if (!Waiting.IsEmpty() && Shown.IsEmpty())
	{
		const FModularMessage Next = Waiting[0];
		Waiting.RemoveAt(0);
		Show(Next);
	}
}
