// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GTO : ModuleRules
{
	public GTO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Headers are included by their path below the module root, e.g. "Vehicle/GTOVehiclePawn.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GTOSim" });

		// Chaos and PhysicsCore for the async physics handle of the vehicle adapter, ProceduralMeshComponent for the track.
		PrivateDependencyModuleNames.AddRange(new string[] { "Chaos", "PhysicsCore", "ProceduralMeshComponent" });

		if (Target.bBuildEditor)
		{
			// The smoke test ends its PIE session.
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
