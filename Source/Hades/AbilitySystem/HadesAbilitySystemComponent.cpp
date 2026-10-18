#include "HadesAbilitySystemComponent.h"


void UHadesAbilitySystemComponent::ToggleAbilitiesByTag(const FGameplayTagContainer& AbilityTags)
{

	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTags, MatchingSpecs, /*bOnlyAbilitiesThatSatisfyTagRequirements*/ false);

	const bool bAnyActive = MatchingSpecs.ContainsByPredicate([](const FGameplayAbilitySpec* Spec)
	{
		return Spec->IsActive();
	});

	if (bAnyActive)
	{
		CancelAbilities(&AbilityTags);
	}
	else
	{
		TryActivateAbilitiesByTag(AbilityTags);
	}
}
