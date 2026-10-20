using System.IO;
using UnrealBuildTool;

// Physics package ultragravity: standard C++ in SI and ISO 8855, does not know Unreal. The CMake project
// under ultragravity/ builds the same sources with doctest; the physics tests run there.
public class UltraGravity : ModuleRules
{
	public UltraGravity(ReadOnlyTargetRules Target) : base(Target)
	{
		// The package sources include no Unreal headers, so no Unreal PCH either.
		PCHUsage = PCHUsageMode.NoPCHs;

		// Several sources have helpers of the same name in anonymous namespaces; a unity build would merge them.
		bUseUnity = false;

		// Public headers are included as "ultragravity/<name>.h" (Public/ is on the include path automatically).
		// nlohmann json stays a detail of the loading code and is not visible outside.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));
		PrivateDefinitions.Add("JSON_NOEXCEPTION");

		// Only for IMPLEMENT_MODULE in UltraGravityModule.cpp; the package sources depend on no Unreal module.
		PrivateDependencyModuleNames.Add("Core");
	}
}
