using UnrealBuildTool;

public class ModularInventoryEditor : ModuleRules
{
	public ModularInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "GameplayTags", "DeveloperSettings",
			"UnrealEd", "PropertyEditor", "AssetTools", "AssetRegistry", "ContentBrowser", "ContentBrowserData", "ToolMenus",
			"MessageLog", "Settings", "DataValidation",
			"ModularInventory"
		});
	}
}
