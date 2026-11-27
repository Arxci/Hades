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
#include "Equipment/InventoryEquipmentComponent.h"
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

	// A banner names the item while the player looks at it, along with its marker.
	Interactable = CreateDefaultSubobject<UHadesInteractableComponent>(TEXT("Interactable"));
	Interactable->SetDisabledText(LOCTEXT("NoRoomPrompt", "Not enough space"));
	Interactable->SetShowPromptMessage(true);

	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));

	TakeText = LOCTEXT("TakePrompt", "Take");
	SwapText = LOCTEXT("SwapPrompt", "Swap");
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
	RefreshRoom();
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

	// A pouch is a capacity upgrade the taker applies as it's taken: it grows the inventory and is used up, so it never needs room.
	// New things to hold go on a free quick slot, as weapons do when they're found. A light source goes into its slot, as that container
	// takes new items, or swaps with the one there, which is left here in its place.
	UHadesQuickSlotComponent* QuickSlots = Interactor->FindComponentByClass<UHadesQuickSlotComponent>();
	const UHadesInventoryComponent* HadesTaker = Cast<UHadesInventoryComponent>(Taker);
	UInventoryEquipmentComponent* TakerEquipment = UInventoryEquipmentComponent::FindEquipmentComponent(Interactor);
	bool bSwapped = false;
	for (const FInventoryItemHandle& Item : GetItems())
	{
		EInventoryError Error;
		if (Taker->TakeItem(Item, 0, true, Error))
		{
			if (QuickSlots && HadesTaker && HadesTaker->CanDraw(Item))
			{
				QuickSlots->AssignToFreeSlot(Item);
			}
			continue;
		}

		const FGameplayTag SwapSlot = TakerEquipment ? FindSwapSlot(*Taker, Item) : FGameplayTag();
		bSwapped |= SwapSlot.IsValid() && TakerEquipment->EquipItem(Item, SwapSlot, Error);
	}

	if (Inventory->CountItems(FInventoryItemQuery()) == 0)
	{
		Destroy();
	}
	else if (!bSwapped)
	{
		AHadesPlayerController::ShowMessageTo(Interactor, LOCTEXT("NoRoomForRest", "Not enough space for the rest."));
	}
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
	// Taken while the player has room for any of it, a pouch counting as room while the inventory can still grow, as taking it applies it.
	// Swapped while what doesn't fit can take the place of what's in one of the player's equipment slots, such as a second light source.
	const UInventoryComponent* Taker = WatchedTaker.Get();
	bool bCanTake = !Taker || !UInventorySubsystem::Get(this);
	bool bCanSwap = false;
	if (!bCanTake)
	{
		for (const FInventoryItemHandle& Item : GetItems())
		{
			bCanTake |= Taker->CanTakeItem(Item, 0, true);
			bCanSwap |= !bCanTake && FindSwapSlot(*Taker, Item).IsValid();
		}
	}

	const FText& Text = bCanTake || !bCanSwap ? TakeText : SwapText;
	if (!Interactable->GetInteractionText().EqualTo(Text))
	{
		Interactable->SetInteractionText(Text);
	}
	Interactable->SetInteractionEnabled(bCanTake || bCanSwap);
}

FGameplayTag AHadesItemPickup::FindSwapSlot(const UInventoryComponent& Taker, FInventoryItemHandle Item) const
{
	// Only into an equipment container that takes new items, such as the light source slot, and only with something there to come back here:
	// an empty one takes the item as it's taken.
	const UInventoryEquipmentComponent* Equipment = UInventoryEquipmentComponent::FindEquipmentComponent(Taker.GetOwner());
	const FGameplayTag Slot = Equipment ? Equipment->FindSlotFor(Item) : FGameplayTag();
	const FInventoryContainerHandle Container = Slot.IsValid() ? Equipment->FindEquipmentContainer(Slot) : FInventoryContainerHandle();
	const bool bSwaps = Container.IsSet() && Taker.TakesNewItems(Container) && Equipment->GetEquippedItem(Slot).IsSet() && Equipment->CanEquipItem(Item, Slot);
	return bSwaps ? Slot : FGameplayTag();
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

	// The prompt names the item and shows its icon, as the player looks at it. What interacting does is Refresh Room's.
	const UInventoryItemCosmetics* Cosmetics = Definition->Cosmetics;
	if (!Definition->DisplayName.IsEmpty())
	{
		Interactable->SetPromptTitle(Definition->DisplayName);
	}
	if (Cosmetics && !Cosmetics->Icon.IsNull())
	{
		Interactable->SetPromptIcon(Cosmetics->Icon);
	}

	// The mesh shows the first item: as set in the editor for its starting item, and loaded once another comes first, such as one swapped in.
	const bool bShownAlready = ShownMeshDefinition.Get() == Definition || (!ShownMeshDefinition.IsValid() && Mesh->GetStaticMesh());
	if (bShownAlready || !Cosmetics || Cosmetics->WorldMesh.IsNull())
	{
		ShownMeshDefinition = Definition;
		return;
	}
	ShownMeshDefinition = Definition;

	const TSoftObjectPtr<UStaticMesh> WorldMesh = Cosmetics->WorldMesh;
	MeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(WorldMesh.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, WorldMesh]()
	{
		Mesh->SetStaticMesh(WorldMesh.Get());
	}));
}

#undef LOCTEXT_NAMESPACE
