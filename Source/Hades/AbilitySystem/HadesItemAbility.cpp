#include "HadesItemAbility.h"

#include "GameFramework/Actor.h"

#include "Hades/Inventory/HadesInventoryComponent.h"

#include "Equipment/InventoryEquipmentComponent.h"
#include "InventoryComponent.h"

UInventoryEquipmentComponent* UHadesItemAbility::GetEquipmentComponent() const
{
	return UInventoryEquipmentComponent::FindEquipmentComponent(GetAvatarActorFromActorInfo());
}

UHadesInventoryComponent* UHadesItemAbility::GetHadesInventoryComponent() const
{
	return Cast<UHadesInventoryComponent>(UInventoryComponent::FindInventoryComponent(GetAvatarActorFromActorInfo()));
}

FInventoryItemHandle UHadesItemAbility::GetSourceItem() const
{
	// The equipment component granted it, as its spec's source, and knows which item for.
	const UInventoryEquipmentComponent* Equipment = GetEquipmentComponent();
	return Equipment ? Equipment->GetItemForAbility(GetCurrentAbilitySpecHandle()) : FInventoryItemHandle();
}

AActor* UHadesItemAbility::FindSpawnedActor(TSubclassOf<AActor> ActorClass) const
{
	const UInventoryEquipmentComponent* Equipment = GetEquipmentComponent();
	const FInventoryItemHandle Item = GetSourceItem();
	if (!Equipment || !Item.IsSet() || !ActorClass)
	{
		return nullptr;
	}

	for (UObject* Spawned : Equipment->GetSpawnedPieces(Item))
	{
		if (AActor* Actor = Cast<AActor>(Spawned); Actor && Actor->IsA(ActorClass))
		{
			return Actor;
		}
	}
	return nullptr;
}
