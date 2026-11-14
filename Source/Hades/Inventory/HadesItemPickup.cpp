#include "HadesItemPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Hades/Hades.h"
#include "Hades/HadesGameplayTags.h"
#include "Hades/Interaction/HadesInteractableComponent.h"
#include "Hades/Inventory/HadesInventoryComponent.h"
#include "Hades/Inventory/HadesQuickSlotComponent.h"
#include "Hades/Player/HadesPlayerController.h"

#include "Data/InventoryItemCosmetics.h"
#include "Data/InventoryItemDefinition.h"
#include "InventoryComponent.h"
#include "InventorySubsystem.h"

#define LOCTEXT_NAMESPACE "HadesItemPickup"

AHadesItemPickup::AHadesItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	// The interactable gives it query collision on the interaction channel only.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetRootComponent(Mesh);

	Interactable = CreateDefaultSubobject<UHadesInteractableComponent>(TEXT("Interactable"));
	Interactable->SetDisabledText(LOCTEXT("NoRoomPrompt", "Not enough space"));

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
}

void AHadesItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// In the editor, show the first starting item. The level then saves the mesh, so it loads with the level instead of popping in.
	const UWorld* World = GetWorld();
	const TArray<FInventoryItemSpec>& StartingItems = Inventory->GetStartingItems();
	if (!World || World->IsGameWorld() || StartingItems.IsEmpty())
	{
		return;
	}

	const UInventoryItemDefinition* Definition = StartingItems[0].Definition;
	const UInventoryItemCosmetics* Cosmetics = Definition ? Definition->Cosmetics.Get() : nullptr;
	if (UStaticMesh* WorldMesh = Cosmetics ? Cosmetics->WorldMesh.LoadSynchronous() : nullptr)
	{
		Mesh->SetStaticMesh(WorldMesh);
	}
}

void AHadesItemPickup::BeginPlay()
{
	// Components begin play in here, so the inventory has its starting items afterwards.
	Super::BeginPlay();

	Interactable->OnInteractionCompleted.AddDynamic(this, &ThisClass::HandleInteractionCompleted);
	Interactable->OnStateTagChanged.AddDynamic(this, &ThisClass::HandleStateTagChanged);
	Inventory->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);
	ShowFirstItem();
}

void AHadesItemPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UInventoryComponent* Taker = WatchedTaker.Get())
	{
		Taker->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AHadesItemPickup::HandleInteractionCompleted(AActor* Interactor)
{
	UInventoryComponent* Taker = UInventoryComponent::FindInventoryComponent(Interactor);
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	if (!Taker || !Subsystem)
	{
		return;
	}

	UHadesInventoryComponent* HadesTaker = Cast<UHadesInventoryComponent>(Taker);
	UHadesQuickSlotComponent* QuickSlots = Interactor->FindComponentByClass<UHadesQuickSlotComponent>();
	for (const FInventoryItemHandle& Item : GetItems())
	{
		if (HadesTaker && Subsystem->HasItemTag(Item, HadesGameplayTags::Item_Type_Pouch))
		{
			TakePouch(*HadesTaker, Item);
			continue;
		}

		EInventoryError Error;
		if (Taker->TakeItem(Item, 0, true, Error) && QuickSlots)
		{
			QuickSlots->AssignToFreeSlot(Item);
		}
	}

	if (Inventory->CountItems(FInventoryItemQuery()) == 0)
	{
		Destroy();
	}
	else
	{
		AHadesPlayerController::ShowMessageTo(Interactor, LOCTEXT("NoRoomForRest", "Not enough space for the rest."));
	}
}

void AHadesItemPickup::TakePouch(UHadesInventoryComponent& Taker, FInventoryItemHandle Pouch)
{
	UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const UInventoryItemDefinition* Definition = Subsystem->GetItemDefinition(Pouch);
	if (Taker.UpgradePockets())
	{
		AHadesPlayerController::ShowMessageTo(Taker.GetOwner(), FText::Format(LOCTEXT("PocketsGrew", "{0}: you can carry more."), Definition->DisplayName));
	}
	else
	{
		UE_LOG(LogHadesInventory, Warning, TEXT("[Pickup] %s: %s has no bigger pockets left in Pocket Upgrades."), *GetNameSafe(this), *GetNameSafe(Taker.GetOwner()));
	}
	Subsystem->RemoveItem(Pouch);
}

void AHadesItemPickup::HandleStateTagChanged(FGameplayTag Tag, bool bAdded)
{
	// Only the player focuses interactables, so the taker is the local player's pawn.
	if (Tag == HadesGameplayTags::Interactable_State_Focused)
	{
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		WatchTaker(bAdded && Player ? UInventoryComponent::FindInventoryComponent(Player->GetPawn()) : nullptr);
	}
}

void AHadesItemPickup::HandleInventoryChanged(UInventoryComponent* ChangedInventory)
{
	if (ChangedInventory == Inventory)
	{
		ShowFirstItem();
	}
	RefreshRoom();
}

void AHadesItemPickup::WatchTaker(UInventoryComponent* NewTaker)
{
	if (UInventoryComponent* OldTaker = WatchedTaker.Get())
	{
		OldTaker->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}

	WatchedTaker = NewTaker;
	if (NewTaker)
	{
		NewTaker->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	}
	RefreshRoom();
}

void AHadesItemPickup::RefreshRoom()
{
	const UInventoryComponent* Taker = WatchedTaker.Get();
	Interactable->SetInteractionEnabled(!Taker || HasRoomFor(*Taker));
}

bool AHadesItemPickup::HasRoomFor(const UInventoryComponent& Taker) const
{
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	if (!Subsystem)
	{
		return true;
	}

	for (const FInventoryItemHandle& Item : GetItems())
	{
		if (Subsystem->HasItemTag(Item, HadesGameplayTags::Item_Type_Pouch) || Taker.CanTakeItem(Item, 0, true))
		{
			return true;
		}
	}
	return false;
}

TArray<FInventoryItemHandle> AHadesItemPickup::GetItems() const
{
	TArray<FInventoryItemHandle> Items;
	if (const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this))
	{
		for (const FInventoryContainerHandle& Container : Inventory->GetContainers())
		{
			Items.Append(Subsystem->GetContainerItems(Container));
		}
	}
	return Items;
}

void AHadesItemPickup::ShowFirstItem()
{
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const TArray<FInventoryItemHandle> Items = GetItems();
	const UInventoryItemDefinition* Definition = Subsystem && !Items.IsEmpty() ? Subsystem->GetItemDefinition(Items[0]) : nullptr;
	if (!Definition)
	{
		return;
	}

	if (!Definition->DisplayName.IsEmpty())
	{
		Interactable->SetInteractionText(FText::Format(LOCTEXT("TakePrompt", "Take {0}"), Definition->DisplayName));
	}

	const UInventoryItemCosmetics* Cosmetics = Definition->Cosmetics;
	if (Mesh->GetStaticMesh() || !Cosmetics || Cosmetics->WorldMesh.IsNull())
	{
		return;
	}

	const TSoftObjectPtr<UStaticMesh> WorldMesh = Cosmetics->WorldMesh;
	MeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(WorldMesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, WorldMesh]()
	{
		Mesh->SetStaticMesh(WorldMesh.Get());
	}));
}

#undef LOCTEXT_NAMESPACE
