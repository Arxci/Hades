#pragma once

#include "CoreMinimal.h"

#include "Components/PrimitiveComponent.h"
#include "UObject/Object.h"

#include "InventoryEquipmentPiece.generated.h"

class AActor;
class UAnimInstance;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;


/**
 * Something an equipped item puts into the world. Lives in the item's cosmetics, so its assets are soft and loaded before Spawn.
 * For Mutable, use an Actor piece whose actor builds its Customizable Object instance from the item it receives on equip.
 */
UCLASS(Abstract, Const, EditInlineNew, DefaultToInstanced, CollapseCategories)
class MODULARINVENTORY_API UInventoryEquipmentPiece : public UObject
{
	GENERATED_BODY()

public:
	virtual void GatherAssets(TArray<FSoftObjectPath>& OutAssets) const {}

	/** Returns the actor or component it created. Its assets are loaded by the time this is called. */
	virtual UObject* Spawn(AActor* Owner, USceneComponent* Parent) const PURE_VIRTUAL(UInventoryEquipmentPiece::Spawn, return nullptr;);

	virtual void Despawn(UObject* Spawned) const;

	/** A component tag or name on the owner. None attaches to the character mesh, or the root. Pieces of attachments look on their parent item's first piece instead. */
	UPROPERTY(EditDefaultsOnly, Category = "Attachment")
	FName AttachComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Attachment")
	FName AttachSocket;

	UPROPERTY(EditDefaultsOnly, Category = "Attachment")
	FTransform RelativeTransform;

	/** For first-person setups: which view the piece renders in. */
	UPROPERTY(EditDefaultsOnly, Category = "Rendering")
	EFirstPersonPrimitiveType FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

protected:
	void ApplyRendering(UPrimitiveComponent* Primitive) const;
};


UCLASS(meta = (DisplayName = "Actor"))
class MODULARINVENTORY_API UInventoryEquipmentPiece_Actor : public UInventoryEquipmentPiece
{
	GENERATED_BODY()

public:
	virtual void GatherAssets(TArray<FSoftObjectPath>& OutAssets) const override;
	virtual UObject* Spawn(AActor* Owner, USceneComponent* Parent) const override;

	/** Receives the item through the Inventory Equipment Actor interface, if it implements it. */
	UPROPERTY(EditDefaultsOnly, Category = "Piece")
	TSoftClassPtr<AActor> ActorClass;
};


UCLASS(meta = (DisplayName = "Static Mesh"))
class MODULARINVENTORY_API UInventoryEquipmentPiece_StaticMesh : public UInventoryEquipmentPiece
{
	GENERATED_BODY()

public:
	virtual void GatherAssets(TArray<FSoftObjectPath>& OutAssets) const override;
	virtual UObject* Spawn(AActor* Owner, USceneComponent* Parent) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Piece")
	TSoftObjectPtr<UStaticMesh> Mesh;
};


UCLASS(meta = (DisplayName = "Skeletal Mesh"))
class MODULARINVENTORY_API UInventoryEquipmentPiece_SkeletalMesh : public UInventoryEquipmentPiece
{
	GENERATED_BODY()

public:
	virtual void GatherAssets(TArray<FSoftObjectPath>& OutAssets) const override;
	virtual UObject* Spawn(AActor* Owner, USceneComponent* Parent) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Piece")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Copies the pose of the skinned mesh it attaches to, for clothing and armor. */
	UPROPERTY(EditDefaultsOnly, Category = "Piece")
	bool bFollowParentPose = false;

	UPROPERTY(EditDefaultsOnly, Category = "Piece", meta = (EditCondition = "!bFollowParentPose"))
	TSoftClassPtr<UAnimInstance> AnimClass;
};
