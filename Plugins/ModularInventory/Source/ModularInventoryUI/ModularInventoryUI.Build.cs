using UnrealBuildTool;

public class ModularInventoryUI : ModuleRules
{
	public ModularInventoryUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "GameplayTags", "Slate", "SlateCore", "UMG", "CommonUI", "ModularInventory" });
	}
}
