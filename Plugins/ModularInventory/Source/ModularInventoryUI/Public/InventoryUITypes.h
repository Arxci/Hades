#pragma once

#include "CoreMinimal.h"

#include "InventoryRegistry.h"

#include "InventoryUITypes.generated.h"

class UInventorySubsystem;


UENUM(BlueprintType)
enum class EInventoryDropAction : uint8
{
	/** Nothing is held, or nothing is pointed at. */
	None,
	Place,
	/** Joins the stack already there. */
	Merge,
	/** Trades places with the item already there. */
	Swap,
	Invalid
};


/** Hears the world's inventory registry while bound. Unbind it before its owner goes away. */
struct MODULARINVENTORYUI_API FInventoryRegistryListener
{
	void Bind(const UObject* WorldContextObject, FOnInventoryRegistryChanged::FDelegate&& Delegate);
	void Unbind();

private:
	TWeakObjectPtr<UInventorySubsystem> Subsystem;
	FDelegateHandle Handle;
};
