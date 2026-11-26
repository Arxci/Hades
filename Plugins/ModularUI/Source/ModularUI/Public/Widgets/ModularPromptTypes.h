#pragma once

#include "CoreMinimal.h"

#include "ModularPromptTypes.generated.h"

class UCommonButtonBase;
class USlider;
class UTextBlock;


/** What a prompt asks for. */
UENUM(BlueprintType)
enum class EModularPromptKind : uint8
{
	/** Yes or no, such as before discarding something. */
	Confirm,
	/** How many, such as how many to split off a stack, chosen with a slider. */
	Quantity
};


/** A question for the player: what it says, what it asks for, and what its answers are called. */
USTRUCT(BlueprintType)
struct MODULARUI_API FModularPromptRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	EModularPromptKind Kind = EModularPromptKind::Confirm;

	/** A heading over the question, such as the name of the item it's about. Its widget collapses while it's empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FText Title;

	/** The question, such as "Discard the herb?". {Count} is the count chosen so far, and {Max} the most there is to choose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt", meta = (MultiLine = true))
	FText Message;

	/** The confirm button's label, on buttons that are Modular Buttons. Empty keeps the button's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FText ConfirmLabel;

	/** The cancel button's label, on buttons that are Modular Buttons. Empty keeps the button's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FText CancelLabel;

	/** The fewest the player can choose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt", meta = (ClampMin = "0", EditCondition = "Kind == EModularPromptKind::Quantity", EditConditionHides))
	int32 MinCount = 1;

	/** The most the player can choose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt", meta = (ClampMin = "0", EditCondition = "Kind == EModularPromptKind::Quantity", EditConditionHides))
	int32 MaxCount = 1;

	/** The count it starts at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt", meta = (ClampMin = "0", EditCondition = "Kind == EModularPromptKind::Quantity", EditConditionHides))
	int32 DefaultCount = 1;
};


/**
 * The named widgets a prompt is shown in, and the request they show: the question, its answers, and for counts, the slider and the count
 * chosen. Context menus' prompts and prompt widgets share it, so a prompt behaves the same wherever it's shown.
 */
struct MODULARUI_API FModularPromptView
{
	TWeakObjectPtr<UTextBlock> TitleText;
	TWeakObjectPtr<UTextBlock> MessageText;
	TWeakObjectPtr<UCommonButtonBase> ConfirmButton;
	TWeakObjectPtr<UCommonButtonBase> CancelButton;
	TWeakObjectPtr<USlider> QuantitySlider;
	TWeakObjectPtr<UTextBlock> QuantityText;

	/** What Quantity Text says. {Count} is the count chosen, and {Max} the most there is to choose. */
	FText QuantityFormat = INVTEXT("{Count}");

	/** Shows the request in the widgets: the slider set to its range and starting count, and shown only for a count to choose. */
	void Show(const FModularPromptRequest& InRequest);

	/** Says the message and the count again, such as after the slider moved. */
	void Refresh() const;

	/** The count chosen: the slider's, or the request's starting count without one. 0 for a confirmation. */
	int32 GetCount() const;

	const FModularPromptRequest& GetRequest() const { return Request; }

private:
	FModularPromptRequest Request;
};
