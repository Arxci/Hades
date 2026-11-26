#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"

#include "Messages/ModularMessageTypes.h"
#include "ModularUITransition.h"

#include "ModularMessageEntryWidget.generated.h"

class UImage;
class UModularInputKeyWidget;
class UTextBlock;
class UWidgetAnimation;


/**
 * One message in a Modular Message Feed, such as a toast or a banner: its title, text, icon, count and key, each in a named widget, each
 * collapsed while the message has nothing for it. It comes in with Show Transition and Show Animation, and goes with Hide Transition, or the
 * show reversed, and Hide Animation.
 */
UCLASS(Abstract)
class MODULARUI_API UModularMessageEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows a message, or with Merged, a repeat merged into the one shown, such as its count going up. */
	void SetMessage(const FModularMessage& InMessage, bool bMerged);

	UFUNCTION(BlueprintPure, Category = "Message")
	FModularMessage GetMessage() const { return Message; }

	const FModularMessage& GetShownMessage() const { return Message; }

	void PlayShow();
	void PlayHide(TFunction<void()>&& OnHidden);
	bool IsHiding() const { return Motion.IsHiding(); }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface

	/** The message changed: a new one, or with Merged, a repeat, such as to pulse the count. Show anything else about it here, such as by its Style. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Message")
	void OnMessageChanged(const FModularMessage& NewMessage, bool bMerged);

	UPROPERTY(BlueprintReadOnly, Category = "Message", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The message's Body. */
	UPROPERTY(BlueprintReadOnly, Category = "Message", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	/** The message's Icon, at the Image Size set in the designer. */
	UPROPERTY(BlueprintReadOnly, Category = "Message", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** The message's Count, in Count Format. Collapsed for 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Message", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** The key of the message's Input Action, such as the key that picks an item up. */
	UPROPERTY(BlueprintReadOnly, Category = "Message", meta = (BindWidgetOptional))
	TObjectPtr<UModularInputKeyWidget> KeyWidget;

	/** What Count Text says. {Count} is the message's count, such as x{Count}. */
	UPROPERTY(EditAnywhere, Category = "Message")
	FText CountFormat = INVTEXT("x{Count}");

	/** How the entry comes in. It adds to the render opacity and transform it has in the designer. */
	UPROPERTY(EditAnywhere, Category = "Transition")
	FModularUITransition ShowTransition;

	/** The entry goes by playing Show Transition backwards, from wherever it got to. Off, it plays Hide Transition. */
	UPROPERTY(EditAnywhere, Category = "Transition")
	bool bHideReversesShow = false;

	UPROPERTY(EditAnywhere, Category = "Transition", meta = (EditCondition = "!bHideReversesShow", EditConditionHides))
	FModularUITransition HideTransition;

	/** Played as the entry shows. Name an animation ShowAnimation to use one. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ShowAnimation;

	/** Played as the entry goes, which waits for it. Without one, Show Animation plays backwards when Hide Reverses Show is on. */
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> HideAnimation;

public:
	UModularMessageEntryWidget(const FObjectInitializer& ObjectInitializer);

private:
	FModularMessage Message;
	FModularMessageDisplay Display;
	FModularMessageMotion Motion;
};
