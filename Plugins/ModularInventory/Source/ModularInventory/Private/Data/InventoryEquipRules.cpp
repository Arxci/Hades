#include "Data/InventoryEquipRules.h"

#if WITH_EDITOR
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "InventoryEquipRules"

EDataValidationResult UInventoryEquipRules::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (Slots.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoSlots", "No slots are allowed, so items using these rules can never be equipped."));
	}

	for (int32 Index = 0; Index < Abilities.Num(); ++Index)
	{
		if (!Abilities[Index].Ability)
		{
			Context.AddError(FText::Format(LOCTEXT("MissingAbility", "Ability {0} has no class."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 Index = 0; Index < Effects.Num(); ++Index)
	{
		if (!Effects[Index].Effect)
		{
			Context.AddError(FText::Format(LOCTEXT("MissingEffect", "Effect {0} has no class."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 Index = 0; Index < AttributeSets.Num(); ++Index)
	{
		if (!AttributeSets[Index])
		{
			Context.AddError(FText::Format(LOCTEXT("MissingAttributeSet", "Attribute Set {0} has no class."), FText::AsNumber(Index)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
