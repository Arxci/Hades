#pragma once

#include "CoreMinimal.h"

#include "ModularActivatableWidget.h"
#include "Widgets/ModularPromptTypes.h"

#include "ModularPromptWidget.generated.h"

class UCommonButtonBase;
class USlider;
class UTextBlock;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FModularPromptAnsweredSignature, bool, bConfirmed, int32, Count);


/**
 * A dialog that asks the player something: to confirm, or how many. Push it onto a layer, such as Modal, and give it a request with Set
 * Request, or use Show Modular Prompt from Blueprint, which does both and waits for the answer. Context menus whose Prompt Presentation is
 * Dialog push one for their entries' prompts.
 * Confirm Button answers yes with the count chosen, and Cancel Button, Back and a right-click answer no. It closes once answered.
 * Give the root a full-screen, hit-testable backdrop, so nothing behind the dialog can be pressed while it's open, and lay the dialog
 * itself out in Prompt Panel, so a click off it can cancel it.
 */
UCLASS(Abstract, Blueprintable)
class MODULARUI_API UModularPromptWidget : public UModularActivatableWidget
{
	GENERATED_BODY()

public:
	UModularPromptWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows the question. Call it before the dialog shows, such as from the push's initializer, or after to change it. */
	UFUNCTION(BlueprintCallable, Category = "Prompt")
	void SetRequest(const FModularPromptRequest& InRequest);

	/** Called once with the answer, as the dialog starts to close. For C++, along with On Answered. */
	void SetOnAnswered(TFunction<void(bool bConfirmed, int32 Count)>&& InOnAnswered);

	/** Answers yes, with the count chosen. */
	UFUNCTION(BlueprintCallable, Category = "Prompt")
	void Confirm();

	/** Answers no. */
	UFUNCTION(BlueprintCallable, Category = "Prompt")
	void Cancel();

	/** The count chosen so far: the slider's, or 0 for a confirmation. */
	UFUNCTION(BlueprintPure, Category = "Prompt")
	int32 GetChosenCount() const;

	UFUNCTION(BlueprintPure, Category = "Prompt")
	FModularPromptRequest GetRequest() const;

	/** The player answered, or the dialog closed some other way, which answers no. */
	UPROPERTY(BlueprintAssignable, Category = "Prompt")
	FModularPromptAnsweredSignature OnAnswered;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	//~ Begin UCommonActivatableWidget Interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	//~ End UCommonActivatableWidget Interface

	/** Set Request was called. Show anything else the request says here, such as a different look for a quantity. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prompt")
	void OnRequestChanged(const FModularPromptRequest& Request);

	/** The dialog itself. A press off it is a press outside, which cancels with Click Outside Cancels. Without it, presses only reach buttons. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PromptPanel;

	/** The request's Title, collapsed while it's empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The request's Message. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	/** Answers yes. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ConfirmButton;

	/** Answers no. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CancelButton;

	/** Chooses how many, for a quantity. Gamepad focus starts on it, so left and right change the count. Collapsed for a confirmation. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<USlider> QuantitySlider;

	/** The count chosen, in Quantity Format. Collapsed for a confirmation. */
	UPROPERTY(BlueprintReadOnly, Category = "Prompt", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	/** What Quantity Text says. {Count} is the count chosen, and {Max} the most there is to choose. */
	UPROPERTY(EditDefaultsOnly, Category = "Prompt")
	FText QuantityFormat = INVTEXT("{Count}");

	/** A confirmation starts on Cancel Button, so the quick answer is the safe one, such as before discarding. Off, it starts on Confirm Button. */
	UPROPERTY(EditDefaultsOnly, Category = "Prompt")
	bool bConfirmStartsOnCancel = true;

	/** A right-click anywhere answers no, as Back does, for players whose right mouse button means Back. */
	UPROPERTY(EditDefaultsOnly, Category = "Prompt")
	bool bRightClickCancels = true;

	/** A left-click off Prompt Panel answers no. Off, the dialog has to be answered. */
	UPROPERTY(EditDefaultsOnly, Category = "Prompt")
	bool bClickOutsideCancels = false;

private:
	UFUNCTION()
	void HandleQuantityChanged(float Value);

	void Answer(bool bConfirmed, bool bClose);

	FModularPromptView View;
	TFunction<void(bool, int32)> OnAnsweredNative;
	bool bAnswered = false;
};
