#pragma once

#include "CoreMinimal.h"

#include "Actions/InventoryItemAction.h"
#include "Actions/InventoryItemActions.h"

#include "HadesItemActions.generated.h"

class UHadesInventoryComponent;


/** An item action on the player's Hades Inventory Component. */
UCLASS(Abstract)
class HADES_API UHadesItemAction : public UInventoryItemAction
{
	GENERATED_BODY()

protected:
	static UHadesInventoryComponent* GetPlayerInventory(const FInventoryItemActionContext& Context);
};


/** Draws the item into a hand. */
UCLASS(meta = (DisplayName = "Equip"))
class HADES_API UHadesItemAction_Equip : public UHadesItemAction
{
	GENERATED_BODY()

public:
	UHadesItemAction_Equip();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** Puts the item in hand away. */
UCLASS(meta = (DisplayName = "Unequip"))
class HADES_API UHadesItemAction_Unequip : public UHadesItemAction
{
	GENERATED_BODY()

public:
	UHadesItemAction_Unequip();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** How Combine has the player say what to make. */
UENUM(BlueprintType)
enum class EHadesCombineStyle : uint8
{
	/**
	 * A list of what the item makes, in Menu Class, with what each takes and how many the player has, to pick one. What no recipe takes,
	 * such as a gun and its ammo, still has the player choose the other item.
	 */
	RecipeList,
	/** The player chooses the other item, and the two make whatever a recipe makes of them, as classic survival horror combines. */
	ChooseItem
};


/**
 * Makes something from the item and the player's other items, from the player's Combine Recipes, as Style says: from a list of what the
 * item makes, in Menu Class, or by choosing the other item. A gun or its ammo, which no recipe takes, always has the player choose the other
 * one, and the Hades inventory screen loads the gun once it's chosen. A Recipe List without a Menu Class chooses the other item too.
 */
UCLASS(meta = (DisplayName = "Combine"))
class HADES_API UHadesItemAction_Combine : public UInventoryItemAction_Craft
{
	GENERATED_BODY()

public:
	UHadesItemAction_Combine();

	EHadesCombineStyle GetStyle() const { return Style; }

protected:
	/** How the player says what to make: from a list of what the item makes, or by choosing the other item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EHadesCombineStyle Style = EHadesCombineStyle::RecipeList;

	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
	virtual const UInventoryRecipeBook* GetRecipeBook(const FInventoryItemActionContext& Context) const override;
};


/** Puts the item in the item box, on an inventory screen that shows it. Key items stay with the player. */
UCLASS(meta = (DisplayName = "Store"))
class HADES_API UHadesItemAction_Store : public UHadesItemAction
{
	GENERATED_BODY()

public:
	UHadesItemAction_Store();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** Takes the item out of the item box, on an inventory screen that shows it. */
UCLASS(meta = (DisplayName = "Take"))
class HADES_API UHadesItemAction_Take : public UHadesItemAction
{
	GENERATED_BODY()

public:
	UHadesItemAction_Take();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};
