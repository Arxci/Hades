#include "Actions/InventoryItemActions.h"

#include "Data/InventoryItemDefinition.h"
#include "InventorySubsystem.h"
#include "InventoryUISubsystem.h"

#define LOCTEXT_NAMESPACE "InventoryItemActions"

// Move
UInventoryItemAction_Move::UInventoryItemAction_Move()
{
	DisplayName = LOCTEXT("MoveName", "Move");
	Description = LOCTEXT("MoveDescription", "Pick the item up to put it somewhere else.");
}

bool UInventoryItemAction_Move::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryUISubsystem* Interaction = GetInteraction(Context);
	return Interaction && Interaction->CanBeginHold(Context.Item);
}

void UInventoryItemAction_Move::Run_Implementation(const FInventoryItemActionContext& Context)
{
	// As the Move action does: held by the cell aimed at when that's one of its own, or else by its middle cell, where it lies.
	if (UInventoryUISubsystem* Interaction = GetInteraction(Context))
	{
		Interaction->SetHoveredItem(Context.Item);
		Interaction->PickUpOrPlace();
	}
}

// Split
UInventoryItemAction_Split::UInventoryItemAction_Split()
{
	DisplayName = LOCTEXT("SplitName", "Split");
	Description = LOCTEXT("SplitDescription", "Take part of the stack to put somewhere else.");
	Prompt = EInventoryActionPrompt::Quantity;
	PromptText = LOCTEXT("SplitPrompt", "Take how many?");
}

bool UInventoryItemAction_Split::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryUISubsystem* Interaction = GetInteraction(Context);
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const UInventoryItemDefinition* Definition = Inventory ? Inventory->GetItemDefinition(Context.Item) : nullptr;
	return Interaction && Definition && Definition->CanSplit() && Inventory->GetItemCount(Context.Item) > 1 && Interaction->CanBeginHold(Context.Item);
}

void UInventoryItemAction_Split::GetQuantityRange_Implementation(const FInventoryItemActionContext& Context, int32& OutMin, int32& OutMax, int32& OutDefault) const
{
	// All but one, as the rest stays where it is.
	const UInventorySubsystem* Inventory = UInventorySubsystem::Get(GetContextObject(Context));
	const int32 Count = Inventory ? Inventory->GetItemCount(Context.Item) : 2;
	OutMin = 1;
	OutMax = FMath::Max(1, Count - 1);
	OutDefault = FMath::Max(1, Count / 2);
}

void UInventoryItemAction_Split::Run_Implementation(const FInventoryItemActionContext& Context)
{
	// Picked up as Move does, so it's held by the same cell, then cut down to the count chosen.
	UInventoryUISubsystem* Interaction = GetInteraction(Context);
	if (!Interaction)
	{
		return;
	}
	Interaction->SetHoveredItem(Context.Item);
	if (Interaction->PickUpOrPlace() && Context.Count > 0)
	{
		Interaction->SetHeldCount(Context.Count);
	}
}

// Transfer
UInventoryItemAction_Transfer::UInventoryItemAction_Transfer()
{
	DisplayName = LOCTEXT("TransferName", "Transfer");
	Description = LOCTEXT("TransferDescription", "Move the item to the other side.");
}

bool UInventoryItemAction_Transfer::CanRun_Implementation(const FInventoryItemActionContext& Context) const
{
	const UInventoryUISubsystem* Interaction = GetInteraction(Context);
	return Interaction && Interaction->CanTransferItem(Context.Item);
}

void UInventoryItemAction_Transfer::Run_Implementation(const FInventoryItemActionContext& Context)
{
	if (UInventoryUISubsystem* Interaction = GetInteraction(Context))
	{
		Interaction->TransferItem(Context.Item);
	}
}

#undef LOCTEXT_NAMESPACE
