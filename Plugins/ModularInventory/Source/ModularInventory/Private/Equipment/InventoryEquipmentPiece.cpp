#include "Equipment/InventoryEquipmentPiece.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

namespace
{
	void AddIfSet(TArray<FSoftObjectPath>& OutAssets, const FSoftObjectPath& Path)
	{
		if (Path.IsValid())
		{
			OutAssets.Add(Path);
		}
	}

	template <typename ComponentType>
	ComponentType* CreateAttachedComponent(AActor* Owner, USceneComponent* Parent, FName Socket, const FTransform& RelativeTransform)
	{
		ComponentType* Component = NewObject<ComponentType>(Owner);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetupAttachment(Parent, Socket);
		Component->SetRelativeTransform(RelativeTransform);
		return Component;
	}
}

void UInventoryEquipmentPiece::Despawn(UObject* Spawned) const
{
	if (AActor* Actor = Cast<AActor>(Spawned))
	{
		Actor->Destroy();
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(Spawned))
	{
		Component->DestroyComponent();
	}
}

void UInventoryEquipmentPiece::ApplyRendering(UPrimitiveComponent* Primitive) const
{
	if (Primitive && FirstPersonPrimitiveType != EFirstPersonPrimitiveType::None)
	{
		Primitive->SetFirstPersonPrimitiveType(FirstPersonPrimitiveType);
	}
}

// Actor
void UInventoryEquipmentPiece_Actor::GatherAssets(TArray<FSoftObjectPath>& OutAssets) const
{
	AddIfSet(OutAssets, ActorClass.ToSoftObjectPath());
}

UObject* UInventoryEquipmentPiece_Actor::Spawn(AActor* Owner, USceneComponent* Parent) const
{
	UClass* Class = ActorClass.Get();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!Class || !World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Cast<APawn>(Owner);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Actor = World->SpawnActor<AActor>(Class, FTransform::Identity, SpawnParams);
	if (!Actor)
	{
		return nullptr;
	}

	if (Parent)
	{
		Actor->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	}
	Actor->SetActorRelativeTransform(RelativeTransform);

	Actor->ForEachComponent<UPrimitiveComponent>(false, [this](UPrimitiveComponent* Primitive)
	{
		ApplyRendering(Primitive);
	});

	return Actor;
}

// Static mesh
void UInventoryEquipmentPiece_StaticMesh::GatherAssets(TArray<FSoftObjectPath>& OutAssets) const
{
	AddIfSet(OutAssets, Mesh.ToSoftObjectPath());
}

UObject* UInventoryEquipmentPiece_StaticMesh::Spawn(AActor* Owner, USceneComponent* Parent) const
{
	UStaticMesh* LoadedMesh = Mesh.Get();
	if (!Owner || !LoadedMesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = CreateAttachedComponent<UStaticMeshComponent>(Owner, Parent, AttachSocket, RelativeTransform);
	Component->SetStaticMesh(LoadedMesh);
	ApplyRendering(Component);
	Component->RegisterComponent();
	return Component;
}

// Skeletal mesh
void UInventoryEquipmentPiece_SkeletalMesh::GatherAssets(TArray<FSoftObjectPath>& OutAssets) const
{
	AddIfSet(OutAssets, Mesh.ToSoftObjectPath());
	if (!bFollowParentPose)
	{
		AddIfSet(OutAssets, AnimClass.ToSoftObjectPath());
	}
}

UObject* UInventoryEquipmentPiece_SkeletalMesh::Spawn(AActor* Owner, USceneComponent* Parent) const
{
	USkeletalMesh* LoadedMesh = Mesh.Get();
	if (!Owner || !LoadedMesh)
	{
		return nullptr;
	}

	USkeletalMeshComponent* Component = CreateAttachedComponent<USkeletalMeshComponent>(Owner, Parent, AttachSocket, RelativeTransform);
	Component->SetSkeletalMeshAsset(LoadedMesh);
	if (!bFollowParentPose && AnimClass.Get())
	{
		Component->SetAnimInstanceClass(AnimClass.Get());
	}
	ApplyRendering(Component);
	Component->RegisterComponent();

	if (USkinnedMeshComponent* Leader = bFollowParentPose ? Cast<USkinnedMeshComponent>(Parent) : nullptr)
	{
		Component->SetLeaderPoseComponent(Leader);
	}
	return Component;
}
