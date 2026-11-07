#pragma once

#include "CoreMinimal.h"

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "InventoryItemCosmetics.generated.h"

class UAnimInstance;
class UInventoryEquipmentPiece;
class UStaticMesh;
class UTexture2D;


USTRUCT(BlueprintType)
struct MODULARINVENTORY_API FInventoryAnimLayer
{
	GENERATED_BODY()

	/** Component tag or name of the skeletal mesh to link on. None uses the character mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	FName TargetMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftClassPtr<UAnimInstance> LayerClass;
};


/** How an item looks. Everything is a soft reference, so loading item definitions never pulls in meshes, textures or animation. */
UCLASS(BlueprintType, Const)
class MODULARINVENTORY_API UInventoryItemCosmetics : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Everything equipping needs loaded: piece assets and anim layers. */
	void GatherEquipAssets(TArray<FSoftObjectPath>& OutAssets) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Shown by pickups lying in the world. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	/** Spawned while the item is equipped. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Equipment")
	TArray<TObjectPtr<UInventoryEquipmentPiece>> Pieces;

	/** Linked while the item is equipped. When several equipped items link the same layer interface, the last equipped wins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (TitleProperty = "LayerClass"))
	TArray<FInventoryAnimLayer> AnimLayers;

	/** Any other cosmetic assets, found by tag: sounds, effects, materials. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Assets")
	TMap<FGameplayTag, TSoftObjectPtr<UObject>> Assets;
};
