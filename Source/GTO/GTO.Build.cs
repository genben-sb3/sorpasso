// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class GTO : ModuleRules
{
	public GTO(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Headers are included by their path below the module root, e.g. "Vehicle/GTOVehiclePawn.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UltraGravity" });

		// Chaos and PhysicsCore for the async physics handle of the vehicle adapter, ProceduralMeshComponent for the track.
		PrivateDependencyModuleNames.AddRange(new string[] { "Chaos", "PhysicsCore", "ProceduralMeshComponent" });

		// The smoke tests compare the car in Chaos with the physics package's test world, which lives in the CMake project
		// and is compiled into the tests from there (Tests/GTOTestWorld.cpp).
		string TestWorld = Path.Combine(ModuleDirectory, "..", "..", "ultragravity", "test_world");
		PrivateIncludePaths.Add(Path.Combine(TestWorld, "include"));
		PrivateIncludePaths.Add(Path.Combine(TestWorld, "src"));

		if (Target.bBuildEditor)
		{
			// The smoke test ends its PIE session.
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
