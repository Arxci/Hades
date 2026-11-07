#pragma once

#include "CoreMinimal.h"

#include "IDetailCustomization.h"


/** Replaces a loot table's Simulate button and dialog with drop rates that update as tables are edited. */
class FInventoryLootTableCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface
};
