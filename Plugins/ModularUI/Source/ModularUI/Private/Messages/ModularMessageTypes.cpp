#include "Messages/ModularMessageTypes.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"

#include "Widgets/ModularInputKeyWidget.h"

namespace
{
	void ShowText(UTextBlock* TextBlock, const FText& Text)
	{
		if (TextBlock)
		{
			TextBlock->SetText(Text);
			TextBlock->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
}

// Message
bool FModularMessage::IsRepeatOf(const FModularMessage& Other) const
{
	if (!Key.IsNone() || !Other.Key.IsNone())
	{
		return Key == Other.Key && Channel == Other.Channel;
	}
	return Channel == Other.Channel && Title.EqualTo(Other.Title) && Body.EqualTo(Other.Body) && Icon == Other.Icon && Style == Other.Style;
}

// Display
FModularMessageDisplay::~FModularMessageDisplay()
{
	CancelLoad();
}

void FModularMessageDisplay::CancelLoad()
{
	if (IconHandle.IsValid() && IconHandle->IsLoadingInProgress())
	{
		IconHandle->CancelHandle();
	}
	IconHandle.Reset();
}

void FModularMessageDisplay::Show(const FModularMessage& Message)
{
	ShowText(TitleText.Get(), Message.Title);
	ShowText(MessageText.Get(), Message.Body);
	ShowText(CountText.Get(), Message.Count > 0 ? FText::FormatNamed(CountFormat, TEXT("Count"), FText::AsNumber(Message.Count)) : FText::GetEmpty());

	if (UModularInputKeyWidget* Key = KeyWidget.Get())
	{
		Key->SetInputAction(Message.InputAction);
	}

	CancelLoad();
	UImage* Image = IconImage.Get();
	if (!Image)
	{
		return;
	}
	if (Message.Icon.IsNull())
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (UTexture2D* Loaded = Message.Icon.Get())
	{
		Image->SetBrushFromTexture(Loaded, /*bMatchSize*/ false);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	// Hidden, not collapsed, while it loads, so the message doesn't change shape as the icon arrives.
	Image->SetVisibility(ESlateVisibility::Hidden);
	const TWeakObjectPtr<UImage> WeakImage = Image;
	const TSoftObjectPtr<UTexture2D> Icon = Message.Icon;
	IconHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Icon.ToSoftObjectPath(), FStreamableDelegate::CreateLambda([WeakImage, Icon]()
	{
		UImage* LoadedInto = WeakImage.Get();
		UTexture2D* Texture = Icon.Get();
		if (LoadedInto && Texture)
		{
			LoadedInto->SetBrushFromTexture(Texture, /*bMatchSize*/ false);
			LoadedInto->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}));
}

// Motion
FModularMessageMotion::~FModularMessageMotion()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
}

void FModularMessageMotion::Init(UUserWidget& InOwner, UWidget& InTarget, UWidgetAnimation* InShowAnimation, UWidgetAnimation* InHideAnimation)
{
	Owner = &InOwner;
	ShowAnimation = InShowAnimation;
	HideAnimation = InHideAnimation;
	Player.Init(&InTarget, &InTarget);
}

void FModularMessageMotion::PlayShow(const FModularUITransition& ShowTransition)
{
	Stop();
	Player.PlayIn(ShowTransition);
	UUserWidget* OwnerWidget = Owner.Get();
	UWidgetAnimation* Show = ShowAnimation.Get();
	if (OwnerWidget && Show)
	{
		if (UWidgetAnimation* Hide = HideAnimation.Get())
		{
			OwnerWidget->StopAnimation(Hide);
		}
		OwnerWidget->PlayAnimation(Show);
	}
	StartTicker();
}

void FModularMessageMotion::PlayHide(bool bHideReversesShow, const FModularUITransition& HideTransition, TFunction<void()>&& OnHidden)
{
	bHiding = true;
	AfterHidden = MoveTemp(OnHidden);
	if (bHideReversesShow)
	{
		Player.ReverseIn();
	}
	else
	{
		Player.PlayOut(HideTransition);
	}

	// Hide Animation, or else Show Animation going back from wherever it got to.
	UUserWidget* OwnerWidget = Owner.Get();
	UWidgetAnimation* Show = ShowAnimation.Get();
	UWidgetAnimation* Hide = HideAnimation.Get();
	AwaitedAnimation.Reset();
	if (OwnerWidget && Hide)
	{
		AwaitedAnimation = Hide;
		OwnerWidget->PlayAnimation(Hide);
	}
	else if (OwnerWidget && Show && bHideReversesShow)
	{
		AwaitedAnimation = Show;
		if (OwnerWidget->IsAnimationPlaying(Show))
		{
			OwnerWidget->ReverseAnimation(Show);
		}
		else
		{
			OwnerWidget->PlayAnimationReverse(Show);
		}
	}
	StartTicker();
}

void FModularMessageMotion::Stop()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle = FTSTicker::FDelegateHandle();
	}
	Player.Stop();
	bHiding = false;
	AfterHidden = TFunction<void()>();
	AwaitedAnimation.Reset();
}

void FModularMessageMotion::StartTicker()
{
	UUserWidget* OwnerWidget = Owner.Get();
	if (TickHandle.IsValid() || !OwnerWidget)
	{
		return;
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(OwnerWidget, [this](float DeltaTime)
	{
		const bool bMoving = Player.Advance(DeltaTime);
		UUserWidget* Animated = Owner.Get();
		const UWidgetAnimation* Awaited = AwaitedAnimation.Get();
		if (Awaited && (!Animated || !Animated->IsAnimationPlaying(Awaited)))
		{
			AwaitedAnimation.Reset();
		}
		if (bMoving || AwaitedAnimation.IsValid())
		{
			return true;
		}

		// Cleared before calling back, as hiding can show the widget again, with a ticker of its own.
		TickHandle = FTSTicker::FDelegateHandle();
		if (bHiding)
		{
			bHiding = false;
			TFunction<void()> OnHidden = MoveTemp(AfterHidden);
			AfterHidden = TFunction<void()>();
			if (OnHidden)
			{
				OnHidden();
			}
		}
		return false;
	}));
}
