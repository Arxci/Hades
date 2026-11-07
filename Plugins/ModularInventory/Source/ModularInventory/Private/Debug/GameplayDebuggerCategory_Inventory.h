#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"

#include "GameplayDebuggerCategory.h"

#include "InventoryTypes.h"

class AActor;
class APlayerController;
struct FInventoryRegistry;


/** Shows the debug actor's containers, items and equipment, or the player's when nothing is selected. */
class FGameplayDebuggerCategory_Inventory : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Inventory();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

private:
	void AddContainerLines(const FInventoryRegistry& Registry, FInventoryContainerHandle Container, int32 Depth);
	void AddGridLines(const FInventoryRegistry& Registry, FInventoryContainerHandle Container, int32 Depth);
	void AddEquipmentLines(const AActor& Target);
	void AddLine(const FString& Line);

	int32 NumLines = 0;
};

#endif // WITH_GAMEPLAY_DEBUGGER
