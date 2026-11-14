#include "Data/InventoryEquipRules.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Algo/AllOf.h"

bool UInventoryEquipRules::MeetsOwnerRequirements(const AActor* Owner) const
{
	// Without an ability system there is nothing to check, which keeps looting corpses and chests simple.
	const bool bHasRequirements = !OwnerRequirements.IsEmpty() || !AttributeRequirements.IsEmpty();
	const UAbilitySystemComponent* AbilitySystem = bHasRequirements ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner) : nullptr;
	if (!AbilitySystem)
	{
		return true;
	}

	FGameplayTagContainer OwnedTags;
	AbilitySystem->GetOwnedGameplayTags(OwnedTags);
	return OwnerRequirements.RequirementsMet(OwnedTags) && Algo::AllOf(AttributeRequirements, [AbilitySystem](const FInventoryAttributeRequirement& Requirement)
	{
		return !Requirement.Attribute.IsValid() || AbilitySystem->GetNumericAttribute(Requirement.Attribute) >= Requirement.MinValue;
	});
}

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

	for (int32 Index = 0; Index < AttributeRequirements.Num(); ++Index)
	{
		if (!AttributeRequirements[Index].Attribute.IsValid())
		{
			Context.AddWarning(FText::Format(LOCTEXT("MissingRequiredAttribute", "Attribute Requirement {0} has no attribute and is ignored."), FText::AsNumber(Index)));
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
