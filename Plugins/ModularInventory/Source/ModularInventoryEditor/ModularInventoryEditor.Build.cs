using UnrealBuildTool;

public class ModularInventoryEditor : ModuleRules
{
	public ModularInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "RenderCore", "InputCore", "ApplicationCore", "Slate", "SlateCore", "GameplayTags", "DeveloperSettings",
			"UnrealEd", "PropertyEditor", "AssetTools", "AssetRegistry", "ContentBrowser", "ContentBrowserData", "ToolMenus",
			"MessageLog", "Settings", "DataValidation", "DesktopPlatform", "Projects", "ImageCore",
			"ModularInventory"
		});
	}
}
