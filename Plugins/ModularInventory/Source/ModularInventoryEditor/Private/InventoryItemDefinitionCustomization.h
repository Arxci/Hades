#pragma once

#include "CoreMinimal.h"

#include "IDetailCustomization.h"


/** Adds a picture of the item's cells and the cells its sockets reach, unrotated and rotated, above its sockets. */
class FInventoryItemDefinitionCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface
};
