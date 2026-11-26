using UnrealBuildTool;

public class ModularUI : ModuleRules
{
	public ModularUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "UMG", "CommonUI", "CommonInput", "DeveloperSettings", "GameplayTags", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore" });
	}
}
