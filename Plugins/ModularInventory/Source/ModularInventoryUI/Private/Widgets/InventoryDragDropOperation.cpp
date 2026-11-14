#include "Widgets/InventoryDragDropOperation.h"

#include "InventoryUISubsystem.h"

void UInventoryDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// Dropped on nothing, or on a spot that refused it. A snapped held item goes where it's drawn, if it can. Nothing is left to drop when
	// the item was put back during the drag, such as with a right-click.
	UInventoryUISubsystem* Subsystem = Interaction.Get();
	if (!Subsystem || !Subsystem->IsHolding()
		|| (Subsystem->GetStyle().bSnapHeldItem && Subsystem->PlaceHeld(Subsystem->GetTargetContainer(), Subsystem->GetTargetPlacement())))
	{
		return;
	}

	// Inventory widgets stop aiming it as the cursor leaves them, so no target means it was let go outside them all.
	if (!Subsystem->GetTargetContainer().IsSet())
	{
		Subsystem->OnDroppedOutside.Broadcast(Subsystem->GetHeldItem(), Subsystem->GetHeldCount());
	}
	Subsystem->CancelHold();
}
