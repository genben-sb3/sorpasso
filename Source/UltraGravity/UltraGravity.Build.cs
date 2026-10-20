using System.IO;
using UnrealBuildTool;

// Physikpaket ultragravity: Standard-C++ in SI und ISO 8855, kennt Unreal nicht. Dieselben Quellen
// baut das CMake-Projekt unter ultragravity/ mit doctest; dort laufen die Physik-Tests.
public class UltraGravity : ModuleRules
{
	public UltraGravity(ReadOnlyTargetRules Target) : base(Target)
	{
		// Die Paket-Quellen binden keine Unreal-Header ein, also auch keinen Unreal-PCH.
		PCHUsage = PCHUsageMode.NoPCHs;

		// Mehrere Quellen haben gleichnamige Helfer in anonymen Namensräumen; ein Unity-Build würde sie zusammenwerfen.
		bUseUnity = false;

		// Öffentliche Header werden als "ultragravity/<name>.h" eingebunden (Public/ liegt automatisch im Include-Pfad).
		// nlohmann json bleibt ein Detail des Ladecodes und ist nach außen nicht sichtbar.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));
		PrivateDefinitions.Add("JSON_NOEXCEPTION");

		// Nur für IMPLEMENT_MODULE in UltraGravityModule.cpp; die Paket-Quellen hängen von keinem Unreal-Modul ab.
		PrivateDependencyModuleNames.Add("Core");
	}
}
