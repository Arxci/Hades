#include "Widgets/InventoryDragDropOperation.h"

#include "InventoryUISubsystem.h"

void UInventoryDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// Dropped on nothing, or on a spot that refused it.
	if (UInventoryUISubsystem* Subsystem = Interaction.Get())
	{
		Subsystem->CancelHold();
	}
}
