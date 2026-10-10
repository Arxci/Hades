#include "HadesInputConfig.h"

#include "InputAction.h"

#include "Hades/Hades.h"

const UInputAction* UHadesInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FHadesInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogHadesInput, Warning, TEXT("[Input] %s has no native input action for %s."),
			*GetNameSafe(this), *InputTag.ToString());
	}
	return nullptr;
}

const FHadesAbilityInputAction* UHadesInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag) const
{
	return AbilityInputActions.FindByPredicate([&InputTag](const FHadesAbilityInputAction& Action)
	{
		return Action.InputTag == InputTag;
	});
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "HadesInputConfig"

EDataValidationResult UHadesInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// Lookups return the first row with a tag, so a tag may only appear once across both lists.
	TSet<FGameplayTag> SeenTags;
	auto ValidateRow = [&](const FText& RowName, const FGameplayTag& InputTag, const UInputAction* InputAction)
	{
		if (!InputTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingTag", "{0} has no Input Tag."), RowName));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			bool bAlreadySeen = false;
			SeenTags.Add(InputTag, &bAlreadySeen);
			if (bAlreadySeen)
			{
				Context.AddError(FText::Format(LOCTEXT("DuplicateTag", "{0} reuses Input Tag {1}."),
					RowName, FText::FromName(InputTag.GetTagName())));
				Result = EDataValidationResult::Invalid;
			}
		}

		if (!InputAction)
		{
			Context.AddError(FText::Format(LOCTEXT("MissingAction", "{0} has no Input Action."), RowName));
			Result = EDataValidationResult::Invalid;
		}
	};

	for (int32 Index = 0; Index < NativeInputActions.Num(); ++Index)
	{
		const FHadesInputAction& Action = NativeInputActions[Index];
		ValidateRow(FText::Format(LOCTEXT("NativeRow", "Native action {0}"), FText::AsNumber(Index)), Action.InputTag, Action.InputAction);
	}

	for (int32 Index = 0; Index < AbilityInputActions.Num(); ++Index)
	{
		const FHadesAbilityInputAction& Action = AbilityInputActions[Index];
		const FText RowName = FText::Format(LOCTEXT("AbilityRow", "Ability action {0}"), FText::AsNumber(Index));
		ValidateRow(RowName, Action.InputTag, Action.InputAction);

		if (!Action.AbilityTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingAbilityTag", "{0} has no Ability Tag."), RowName));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR