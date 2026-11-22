using UnrealBuildTool;

public class ShaderOverridePlugin_InMemoryEditor : ModuleRules
{
	public ShaderOverridePlugin_InMemoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"Projects"
			}
		);
	}
}
