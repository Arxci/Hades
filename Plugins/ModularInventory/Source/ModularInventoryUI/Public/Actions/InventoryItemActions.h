#pragma once

#include "CoreMinimal.h"

#include "Actions/InventoryItemAction.h"

#include "InventoryItemActions.generated.h"


/** Picks the item up to put it down somewhere else, as a drag or a screen's Move action does. Gives item menus a way to move items with a gamepad. */
UCLASS(meta = (DisplayName = "Move"))
class MODULARINVENTORYUI_API UInventoryItemAction_Move : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Move();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** Picks up part of a stack to put down somewhere else. It asks how many by default, offering 1 to all but one. */
UCLASS(meta = (DisplayName = "Split"))
class MODULARINVENTORYUI_API UInventoryItemAction_Split : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Split();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void GetQuantityRange_Implementation(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/** Moves the item to the other side, such as from a chest into the player's storage, while the screen has a transfer container. */
UCLASS(meta = (DisplayName = "Transfer"))
class MODULARINVENTORYUI_API UInventoryItemAction_Transfer : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_Transfer();

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;
};


/**
 * Uses a capacity upgrade the player carries, such as a pouch without Apply When Taken: its Inventory Capacity Upgrade adds room to the
 * player's container, or lets items stack higher there, and one of it is used up. Only offered for items with one, and shown disabled
 * once the container can't grow any more if When Unavailable says so.
 */
UCLASS(meta = (DisplayName = "Apply Capacity Upgrade"))
class MODULARINVENTORYUI_API UInventoryItemAction_ApplyCapacityUpgrade : public UInventoryItemAction
{
	GENERATED_BODY()

public:
	UInventoryItemAction_ApplyCapacityUpgrade();

	/** Only items with an Inventory Capacity Upgrade, on top of Item Filter. */
	virtual bool AppliesTo(const FInventoryItemActionContext& Context) const override;

protected:
	virtual bool CanRun_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual FText GetUnavailableReason_Implementation(const FInventoryItemActionContext& Context) const override;
	virtual void Run_Implementation(const FInventoryItemActionContext& Context) override;

	/** Why it can't run while the container can't grow any more, for an entry shown disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FText AtMaxCapacityReason;
};
