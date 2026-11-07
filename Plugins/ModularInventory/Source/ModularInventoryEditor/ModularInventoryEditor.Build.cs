using UnrealBuildTool;

public class ModularInventoryEditor : ModuleRules
{
	public ModularInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "Slate", "SlateCore", "UnrealEd", "PropertyEditor", "AssetTools", "AssetRegistry", "ContentBrowser", "ContentBrowserData",
			"ToolMenus", "ModularInventory"
		});
	}
}
