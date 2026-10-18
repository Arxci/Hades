#include "HadesInputConfig.h"

#include "InputAction.h"

#include "Hades/Hades.h"

const UInputAction* UHadesInputConfig::FindInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FHadesInputAction& Action : InputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogHadesInput, Warning, TEXT("[Input] %s has no input action for %s."),
			*GetNameSafe(this), *InputTag.ToString());
	}
	return nullptr;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "HadesInputConfig"

EDataValidationResult UHadesInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// Lookups return the first row with a tag, so a tag may only appear once.
	TSet<FGameplayTag> SeenTags;
	for (int32 Index = 0; Index < InputActions.Num(); ++Index)
	{
		const FHadesInputAction& Action = InputActions[Index];
		const FText RowName = FText::Format(LOCTEXT("Row", "Input action {0}"), FText::AsNumber(Index));

		if (!Action.InputTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingTag", "{0} has no Input Tag."), RowName));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			bool bAlreadySeen = false;
			SeenTags.Add(Action.InputTag, &bAlreadySeen);
			if (bAlreadySeen)
			{
				Context.AddError(FText::Format(LOCTEXT("DuplicateTag", "{0} reuses Input Tag {1}."),
					RowName, FText::FromName(Action.InputTag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
		}

		if (!Action.InputAction)
		{
			Context.AddError(FText::Format(LOCTEXT("MissingAction", "{0} has no Input Action."), RowName));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
