#pragma once

#include "CoreMinimal.h"

namespace InventoryEditor
{
	/** The Message Log page the tools list skipped and failed work on. */
	inline const TCHAR* const MessageLogName = TEXT("ModularInventory");

	/** A short toast in the corner of the editor. */
	void Notify(const FText& Message);
}
