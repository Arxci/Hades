using UnrealBuildTool;

public class ModularInventory : ModuleRules
{
	public ModularInventory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayTags", "GameplayAbilities", "GameplayTasks", "GameFeatures" });

		SetupGameplayDebuggerSupport(Target);
	}
}
