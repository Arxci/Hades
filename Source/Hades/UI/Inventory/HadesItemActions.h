#pragma once

#include "CoreMinimal.h"

#include "Actions/InventoryItemAction.h"

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


/** Has the player choose another item to combine this one with. The Hades inventory screen combines them once it's chosen. */
UCLASS(meta = (DisplayName = "Combine"))
class HADES_API UHadesItemAction_Combine : public UHadesItemAction
{
	GENERATED_BODY()

public:
	UHadesItemAction_Combine();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
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
