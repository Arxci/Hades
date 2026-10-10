#include "HadesAbilitySystemComponent.h"


#include "Hades/Hades.h" 
#include "HadesAbility.h" 


UHadesAbilitySystemComponent::UHadesAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHadesAbilitySystemComponent::InitAbilityActorInfo(AController* InController)
{
	CachedController = InController; 
}

void UHadesAbilitySystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	TArray<UHadesAbility*, TInlineAllocator<8>> TickList;
	for (const auto& Pair : GrantedAbilities)
	{
		UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->IsEnabled())
		{
			TickList.Add(Ability);
		}
	}
	for (UHadesAbility* Ability : TickList)
	{
		if (Ability->HasCooldown())
		{
			Ability->TickCooldown(DeltaTime);
		}
		if (Ability->CanTick())
		{
			Ability->TickAbility(DeltaTime);
		}
	}
}

void UHadesAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateAllAbilities();
	Super::EndPlay(EndPlayReason);
}

void UHadesAbilitySystemComponent::HandleAbilityActivated(UHadesAbility* InAbility)
{
	if (!InAbility) return;
	
	AddTags(InAbility->ActivationOwnedTags);
	AddTags(InAbility->AbilityTags);
	
	OnAbilityActivated.Broadcast(InAbility);
}

void UHadesAbilitySystemComponent::HandleAbilityDeactivated(UHadesAbility* InAbility)
{
	if (!InAbility) return;

	if (!InAbility->ActivationOwnedTags.IsEmpty())
	{
		RemoveTags(InAbility->ActivationOwnedTags);
	}

	RemoveTags(InAbility->AbilityTags);
	
	OnAbilityDeactivated.Broadcast(InAbility);
}

//Ability Lifecycle
UHadesAbility* UHadesAbilitySystemComponent::GiveAbility(const TSubclassOf<UHadesAbility> AbilityClass)
{
	if (!AbilityClass) return nullptr;

	if (UHadesAbility* Existing = GrantedAbilities.FindRef(AbilityClass))
	{
		++AbilityGrantCounts.FindOrAdd(AbilityClass);
		if (!Existing->IsEnabled())
		{
			Existing->OnEnableAbility();
		}
		return Existing;
	}

	if (UHadesAbility* NewAbility = NewObject<UHadesAbility>(this, AbilityClass))
	{
		GrantedAbilities.Add(AbilityClass, NewAbility);
		AbilityGrantCounts.Add(AbilityClass, 1);
		NewAbility->OnActivated.AddDynamic(this, &UHadesAbilitySystemComponent::HandleAbilityActivated);
		NewAbility->OnDeactivated.AddDynamic(this, &UHadesAbilitySystemComponent::HandleAbilityDeactivated);
		// Init before broadcast so subscribers see a fully-prepared ability.
		NewAbility->InitializeAbility();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogHadesAbilitySystem, Verbose, TEXT("[ASC] %s granted ability %s."),
			*GetNameSafe(GetOwner()), *GetNameSafe(AbilityClass));
#endif
		OnAbilityGiven.Broadcast(NewAbility);
		return NewAbility;
	}

	UE_LOG(LogHadesAbilitySystem, Warning,
		TEXT("[ASC] Failed to instantiate ability %s on %s."),
		*GetNameSafe(AbilityClass), *GetNameSafe(GetOwner()));
	return nullptr;
}

bool UHadesAbilitySystemComponent::RemoveAbility(const TSubclassOf<UHadesAbility> AbilityClass)
{
	if (!AbilityClass) return false;

	UHadesAbility* Existing = GrantedAbilities.FindRef(AbilityClass);
	if (!Existing) return false;

	if (int32* Count = AbilityGrantCounts.Find(AbilityClass))
	{
		(*Count)--;
		if (*Count > 0)
		{
			// Still granted by another source — keep the instance alive.
			return true;
		}
		AbilityGrantCounts.Remove(AbilityClass);
	}

	if (Existing->IsActive())
	{
		Existing->ForceEndAbility();
	}
	if (Existing->IsEnabled())
	{
		Existing->OnDisableAbility();
	}

	OnAbilityRemoved.Broadcast(Existing);

	Existing->OnActivated.RemoveAll(this);
	Existing->OnDeactivated.RemoveAll(this);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogHadesAbilitySystem, Verbose, TEXT("[ASC] %s removed ability %s."),
		*GetNameSafe(GetOwner()), *GetNameSafe(AbilityClass));
#endif

	GrantedAbilities.Remove(AbilityClass);
	return true;
}

void UHadesAbilitySystemComponent::ClearAbilities()
{
	for (const auto& Pair : GrantedAbilities)
	{
		UHadesAbility* Ability = Pair.Value;
		if (!Ability) continue;

		if (Ability->IsActive())
		{
			Ability->ForceEndAbility();
		}
		if (Ability->IsEnabled())
		{
			Ability->OnDisableAbility();
		}
		OnAbilityRemoved.Broadcast(Ability);

		Ability->OnActivated.Clear();
		Ability->OnDeactivated.Clear();
	}
	GrantedAbilities.Empty();
	AbilityGrantCounts.Empty();
}

bool UHadesAbilitySystemComponent::TryActivateAbilityByClass(const TSubclassOf<UHadesAbility> InAbilityToActivate)
{
	if (!InAbilityToActivate) return false;
	
	UHadesAbility* FoundAbility = GrantedAbilities.FindRef(InAbilityToActivate);
	if (!FoundAbility) return false;
	
	if (!CheckTagRequirements(FoundAbility)) return false;
	
    if (!FoundAbility->RequestActivateAbility()) return false;

	//Attempt to cancel abilities
	CancelAbilitiesWithTags(FoundAbility->CancelAbilitiesWithTags);
	
	return true;
}

bool UHadesAbilitySystemComponent::TryDeactivateAbilityByClass(const TSubclassOf<UHadesAbility> InAbilityToDeactivate)
{
	if (!InAbilityToDeactivate) return false;

	UHadesAbility* FoundAbility = GrantedAbilities.FindRef(InAbilityToDeactivate);
	if (!FoundAbility) return false;

	return FoundAbility->RequestDeactivateAbility();
}

bool UHadesAbilitySystemComponent::TryActivateAbilityByTag(FGameplayTag InAbilityTag)
{
	if (!InAbilityTag.IsValid()) return false;

	for (const auto& Pair : GrantedAbilities)
	{
		UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->AbilityTags.HasTag(InAbilityTag))
		{
			return TryActivateAbilityByClass(Pair.Key);
		}
	}
	return false;
}

bool UHadesAbilitySystemComponent::TryDeactivateAbilityByTag(FGameplayTag InAbilityTag)
{
	if (!InAbilityTag.IsValid()) return false;
	
	for (const auto& Pair : GrantedAbilities)
	{
		UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->AbilityTags.HasTag(InAbilityTag))
		{
			return TryDeactivateAbilityByClass(Pair.Key);
		}
	}
	return false;
}

bool UHadesAbilitySystemComponent::ToggleAbilityByTag(FGameplayTag InAbilityTag)
{
	if (!InAbilityTag.IsValid()) return false;

	for (const auto& Pair : GrantedAbilities)
	{
		const UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->AbilityTags.HasTag(InAbilityTag))
		{
			return Ability->IsActivationRequested()
				? TryDeactivateAbilityByClass(Pair.Key)
				: TryActivateAbilityByClass(Pair.Key);
		}
	}
	return false;
}

void UHadesAbilitySystemComponent::DeactivateAllAbilities()
{
	for (const auto& Pair : GrantedAbilities)
	{
		if (UHadesAbility* Ability = Pair.Value)
		{
			Ability->RequestDeactivateAbility();
		}
	}
}

bool UHadesAbilitySystemComponent::ForceEndAbilityByTag(FGameplayTag InAbilityTag)
{
	if (!InAbilityTag.IsValid()) return false;

	for (const auto& Pair : GrantedAbilities)
	{
		UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->AbilityTags.HasTag(InAbilityTag))
		{
			Ability->ForceEndAbility();
			return true;
		}
	}
	return false;
}

//Utility
UHadesAbility* UHadesAbilitySystemComponent::FindAbilityByClass(TSubclassOf<UHadesAbility> AbilityClass) const
{
	if (!AbilityClass) return nullptr;

	// Exact-key fast path — O(1) when the caller granted and looked up the same
	// concrete class (the common case).
	if (UHadesAbility* Direct = GrantedAbilities.FindRef(AbilityClass))
	{
		return Direct;
	}

	// Subclass walk — when the project granted a BP subclass and a C++ caller
	// looks up by the base type. O(n) but n is the number of granted abilities, which is small.
	for (const TPair<TSubclassOf<UHadesAbility>, TObjectPtr<UHadesAbility>>& Pair : GrantedAbilities)
	{
		UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->IsA(AbilityClass))
		{
			return Ability;
		}
	}
	return nullptr;
}

bool UHadesAbilitySystemComponent::IsAbilityActive(TSubclassOf<UHadesAbility> AbilityClass) const
{
	if (const UHadesAbility* Ability = GrantedAbilities.FindRef(AbilityClass))
	{
		return Ability->IsActive();
	}
	return false;
}

TArray<UHadesAbility*> UHadesAbilitySystemComponent::GetActiveAbilities() const
{
	TArray<UHadesAbility*> Result;
	for (const auto& Pair : GrantedAbilities)
	{
		if (Pair.Value && Pair.Value->IsActive())
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<UHadesAbility*> UHadesAbilitySystemComponent::GetGivenAbilities() const
{
	TArray<UHadesAbility*> Result;
	for (const auto& Pair : GrantedAbilities)
	{
		if (Pair.Value && Pair.Value->IsEnabled())
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

bool UHadesAbilitySystemComponent::IsAbilityActiveByTag(FGameplayTag AbilityTag) const
{
	if (!AbilityTag.IsValid()) return false;
	for (const auto& Pair : GrantedAbilities)
	{
		const UHadesAbility* Ability = Pair.Value;
		if (Ability && Ability->IsActive() && Ability->AbilityTags.HasTag(AbilityTag))
		{
			return true;
		}
	}
	return false;
}


//Tags
void UHadesAbilitySystemComponent::AddTags(const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		int32& Count = TagRefCounts.FindOrAdd(Tag, 0);
		Count++;
		if (Count == 1)
		{
			OwnedTags.AddTag(Tag);
			OnTagChanged.Broadcast(Tag, true);
		}
	}
}

void UHadesAbilitySystemComponent::RemoveTags(const FGameplayTagContainer& Tags)
{
	for (const FGameplayTag& Tag : Tags)
	{
		if (int32* Count = TagRefCounts.Find(Tag))
		{
			(*Count)--;
			if (*Count <= 0)
			{
				OwnedTags.RemoveTag(Tag);
				TagRefCounts.Remove(Tag);
				OnTagChanged.Broadcast(Tag, false);
			}
		}
	}
}

bool UHadesAbilitySystemComponent::HasTag(FGameplayTag Tag) const
{
	return OwnedTags.HasTag(Tag);
}

bool UHadesAbilitySystemComponent::HasAllTags(const FGameplayTagContainer& TagContainer) const
{
	return OwnedTags.HasAll(TagContainer);
}

bool UHadesAbilitySystemComponent::HasAnyTags(const FGameplayTagContainer& TagContainer) const
{
	return OwnedTags.HasAny(TagContainer);
}

FGameplayTagContainer UHadesAbilitySystemComponent::GetOwnedTags() const
{
	return OwnedTags;
}

void UHadesAbilitySystemComponent::AddLooseGameplayTag(FGameplayTag Tag)
{
	if (!Tag.IsValid()) return;

	int32& Count = TagRefCounts.FindOrAdd(Tag, 0);
	Count++;
	if (Count == 1)
	{
		OwnedTags.AddTag(Tag);
		OnTagChanged.Broadcast(Tag, true);
	}
}

void UHadesAbilitySystemComponent::RemoveLooseGameplayTag(FGameplayTag Tag)
{
	if (!Tag.IsValid()) return;

	if (int32* Count = TagRefCounts.Find(Tag))
	{
		(*Count)--;
		if (*Count <= 0)
		{
			OwnedTags.RemoveTag(Tag);
			TagRefCounts.Remove(Tag);
			OnTagChanged.Broadcast(Tag, false);
		}
	}
}

bool UHadesAbilitySystemComponent::CheckTagRequirements(const UHadesAbility* Ability) const
{
	if (!Ability->ActivationRequiredTags.IsEmpty())
	{
		if (!OwnedTags.HasAll(Ability->ActivationRequiredTags))
		{
			return false;
		}
	}
	
	if (!Ability->ActivationBlockedTags.IsEmpty())
	{
		if (OwnedTags.HasAny(Ability->ActivationBlockedTags))
		{
			return false;
		}
	}

	return true;
}

void UHadesAbilitySystemComponent::CancelAbilitiesWithTags(const FGameplayTagContainer& Tags)
{
	for (const auto& tag : Tags)
	{
		TryDeactivateAbilityByTag(tag);
	}
}
