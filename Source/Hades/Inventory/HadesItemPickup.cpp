#include "HadesItemPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

#include "Hades/Interaction/HadesInteractableComponent.h"

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
	ShowFirstItem();
}

void AHadesItemPickup::HandleInteractionCompleted(AActor* Interactor)
{
	UInventoryComponent* Taker = UInventoryComponent::FindInventoryComponent(Interactor);
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	if (!Taker || !Subsystem)
	{
		return;
	}

	for (const FInventoryContainerHandle& Container : Inventory->GetContainers())
	{
		for (const FInventoryItemHandle& Item : Subsystem->GetContainerItems(Container))
		{
			EInventoryError Error;
			Taker->TakeItem(Item, 0, true, Error);
		}
	}

	if (Inventory->CountItems(FInventoryItemQuery()) == 0)
	{
		Destroy();
	}
	else
	{
		ShowFirstItem();
	}
}

void AHadesItemPickup::ShowFirstItem()
{
	const UInventorySubsystem* Subsystem = UInventorySubsystem::Get(this);
	const TArray<FInventoryItemHandle> Items = Inventory->FindItems(FInventoryItemQuery());
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
