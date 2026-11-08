#include "HadesLockComponent.h"

#include "Hades/HadesGameplayTags.h"
#include "Hades/Player/HadesPlayerController.h"

#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventorySubsystem.h"

#define LOCTEXT_NAMESPACE "HadesLock"

UHadesLockComponent::UHadesLockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	LockedText = LOCTEXT("Locked", "It's locked.");
}

bool UHadesLockComponent::TryUnlock(AActor* Unlocker)
{
	if (!bLocked)
	{
		return true;
	}

	FInventoryItemQuery KeyQuery;
	KeyQuery.Definitions.Add(Key);
	const UInventoryComponent* Inventory = UInventoryComponent::FindInventoryComponent(Unlocker);
	const TArray<FInventoryItemHandle> Keys = Key && Inventory ? Inventory->FindItems(KeyQuery) : TArray<FInventoryItemHandle>();
	if (Keys.IsEmpty())
	{
		AHadesPlayerController::ShowMessageTo(Unlocker, LockedText);
		return false;
	}

	bLocked = false;
	UseUpKey(Unlocker, Keys[0]);
	OnUnlocked.Broadcast(Unlocker);
	return true;
}

void UHadesLockComponent::UseUpKey(AActor* Unlocker, FInventoryItemHandle KeyItem)
{
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const FGameplayTag Uses = HadesGameplayTags::Item_Stat_Uses;
	const bool bCountsUses = Subsystem && Subsystem->HasTagValue(KeyItem, Uses);
	if (bCountsUses)
	{
		Subsystem->AddToTagValue(KeyItem, Uses, -1.0f);
	}

	if (bCountsUses && Subsystem->GetTagValue(KeyItem, Uses) <= 0.0f)
	{
		Subsystem->RemoveItem(KeyItem);
		AHadesPlayerController::ShowMessageTo(Unlocker, FText::Format(LOCTEXT("UsedUp", "Used the {0}. It's no longer needed, so you discarded it."), Key->DisplayName));
	}
	else
	{
		AHadesPlayerController::ShowMessageTo(Unlocker, FText::Format(LOCTEXT("Used", "Used the {0}."), Key->DisplayName));
	}
}

#undef LOCTEXT_NAMESPACE
