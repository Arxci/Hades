#pragma once

#include "CoreMinimal.h"

#include "Blueprint/DragDropOperation.h"

#include "InventoryDragDropOperation.generated.h"

class UInventoryUISubsystem;


/** A mouse drag of the held item. The drag only carries the hold; the subsystem knows what is held and where it can go. */
UCLASS()
class MODULARINVENTORYUI_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UInventoryUISubsystem> Interaction;

protected:
	//~ Begin UDragDropOperation Interface
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
	//~ End UDragDropOperation Interface
};
