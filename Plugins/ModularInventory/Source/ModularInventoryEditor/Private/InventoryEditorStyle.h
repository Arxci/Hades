#pragma once

#include "CoreMinimal.h"

class FSlateStyleSet;
class ISlateStyle;


/**
 * The Modular Inventory editor's icons: the plugin's own, on the Level Editor toolbar and the documentation tab, and one for each tool in
 * its menu. They're SVGs in the plugin's Resources/Icons folder. Registered while the editor module is loaded.
 */
class FInventoryEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedRef<FSlateStyleSet> Create();

	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
