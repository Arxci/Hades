#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "ModularInventory.h"

namespace
{
	void GiveItem(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		UInventoryComponent* Inventory = UInventoryComponent::FindInventoryComponent(Player ? Player->GetPawn() : nullptr);
		if (Args.IsEmpty() || !Inventory)
		{
			UE_LOG(LogInventory, Warning, TEXT("Usage: Inventory.Give <Item> [Count], while playing as a pawn with an inventory component."));
			return;
		}

		UAssetManager& AssetManager = UAssetManager::Get();
		const FPrimaryAssetId ItemId(UInventoryItemDefinition::AssetType, FName(*Args[0]));
		UInventoryItemDefinition* Definition = Cast<UInventoryItemDefinition>(AssetManager.GetPrimaryAssetPath(ItemId).TryLoad());
		if (!Definition)
		{
			TArray<FPrimaryAssetId> KnownItems;
			AssetManager.GetPrimaryAssetIdList(UInventoryItemDefinition::AssetType, KnownItems);
			TArray<FString> Names;
			for (const FPrimaryAssetId& Known : KnownItems)
			{
				Names.Add(Known.PrimaryAssetName.ToString());
			}
			Names.Sort();
			UE_LOG(LogInventory, Warning, TEXT("Inventory.Give: no item named %s. Known items: %s"), *Args[0],
				Names.IsEmpty() ? TEXT("none; register items with the Asset Manager first.") : *FString::Join(Names, TEXT(", ")));
			return;
		}

		const int32 Count = Args.IsValidIndex(1) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1;
		TArray<FInventoryItemHandle> Added;
		const int32 AddedCount = Inventory->AddItem(FInventoryItemSpec(Definition, Count), true, Added);
		UE_LOG(LogInventory, Display, TEXT("Inventory.Give: added %d of %d %s."), AddedCount, Count, *Definition->GetName());
	}

	FAutoConsoleCommandWithWorldAndArgs GiveItemCommand(
		TEXT("Inventory.Give"),
		TEXT("Inventory.Give <Item> [Count]: adds items to the local player's inventory. Item is the item definition's asset name."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GiveItem));
}

#endif // !UE_BUILD_SHIPPING
