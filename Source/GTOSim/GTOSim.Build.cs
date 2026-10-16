using UnrealBuildTool;

// Vehicle simulation. The sim core depends on Core only (math and basic types, no UObjects),
// so it can be tested headless and the Unreal adapter can be swapped without touching it.
public class GTOSim : ModuleRules
{
	public GTOSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
