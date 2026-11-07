#pragma once

#include "CoreMinimal.h"

#include "IDetailCustomization.h"


/**
 * Adds to a single item definition's details: a Create button for missing cosmetics, the icon as widgets show it with a button to the Icon Studio,
 * and a picture of the item's cells and the cells its sockets reach.
 */
class FInventoryItemDefinitionCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface
};
