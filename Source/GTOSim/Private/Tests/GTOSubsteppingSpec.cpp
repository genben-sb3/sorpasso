#include "Misc/AutomationTest.h"
#include "GTOSimTestCars.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GTOSimTest;

namespace
{
	// How the Unreal adapter drives the sim core: one Chaos step of 1/120 s, 8 substeps against contacts found once
	// per step, and the chassis moved by Chaos from the forces averaged over the step.
	constexpr double ChaosStep = 1.0 / 120.0;
	constexpr int32 AdapterSubsteps = 8;

	FGTOHeadlessHarness MakeAdapterLikeHarness(const FGTOVehicleParams& Params, const FGTOFlatGround& Ground, const FGTOChassisKinematics& Start)
	{
		return FGTOHeadlessHarness(Params, Ground, Start, ChaosStep, AdapterSubsteps);
	}
}

BEGIN_DEFINE_SPEC(FGTOSubsteppingSpec, "GTO.Sim.Substepping", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOSubsteppingSpec)

void FGTOSubsteppingSpec::Define()
{
	Describe("Stepped like the Unreal adapter, with forces averaged over 8 substeps", [this]()
	{
		It("comes to rest on flat ground without bouncing, jittering or drifting sideways", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOChassisKinematics Start = DroppedAbove(Params, Ground, 0.05);
			Start.Orientation = FQuat(FRotator(0.5, 0.0, 1.0));
			FGTOHeadlessHarness Harness = MakeAdapterLikeHarness(Params, Ground, Start);

			bool bLanded = false;
			bool bHopped = false;
			RunFor(Harness, 2.0, [&]()
			{
				bHopped |= bLanded && AnyWheelUp(Harness.State().Wheels);
				bLanded |= AllWheelsDown(Harness.State().Wheels);
			});

			const FVector SettledPosition = Harness.Chassis().Position;
			double MaxSpeed = 0.0;
			double MaxAngularSpeed = 0.0;
			RunFor(Harness, 2.0, [&]()
			{
				MaxSpeed = FMath::Max(MaxSpeed, Harness.Chassis().LinearVelocity.Size());
				MaxAngularSpeed = FMath::Max(MaxAngularSpeed, Harness.Chassis().AngularVelocity.Size());
			});

			TestTrue(TEXT("lands on all four wheels"), bLanded);
			TestFalse(TEXT("no wheel lifts off again after landing"), bHopped);
			TestTrue(FString::Printf(TEXT("max speed after 2 s (%g m/s) < 1 mm/s"), MaxSpeed), MaxSpeed < 1.0e-3);
			TestTrue(FString::Printf(TEXT("max angular speed after 2 s (%g rad/s) < 1 mrad/s"), MaxAngularSpeed), MaxAngularSpeed < 1.0e-3);
			TestTrue(TEXT("no movement after 2 s"), FVector::Dist(SettledPosition, Harness.Chassis().Position) < 1.0e-3);
			TestTrue(TEXT("no horizontal drift"), FVector2D(Harness.Chassis().Position - Start.Position).Size() < 1.0e-3);
		});

		It("carries the static axle loads where the centre of mass sits", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOHeadlessHarness Harness = MakeAdapterLikeHarness(Params, Ground, DroppedAbove(Params, Ground, 0.05));

			RunFor(Harness, 3.0);

			const FWheelStates& Wheels = Harness.State().Wheels;
			const double Weight = Params.Mass * FGTOHeadlessHarness::Gravity;
			const double FrontLoad = Wheels[FrontLeft].Load + Wheels[FrontRight].Load;
			const double RearLoad = Wheels[RearLeft].Load + Wheels[RearRight].Load;

			TestNearlyEqual(TEXT("front axle load"), FrontLoad, FrontWeightShare * Weight, 0.005 * Weight);
			TestNearlyEqual(TEXT("rear axle load"), RearLoad, (1.0 - FrontWeightShare) * Weight, 0.005 * Weight);
		});

		// Starts with the wheels just touching: a free fall would add Chaos's own 1/120 s integration error
		// (g * dt * t / 2, about 4 mm after a 5 cm drop), which the substepping can neither cause nor fix.
		It("sinks onto its springs like the car stepped at 960 Hz throughout, within 2 mm", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			const FGTOChassisKinematics Start = DroppedAbove(Params, Ground, 0.0);
			FGTOHeadlessHarness Adapter = MakeAdapterLikeHarness(Params, Ground, Start);
			FGTOHeadlessHarness Reference(Params, Ground, Start, ChaosStep / AdapterSubsteps);

			double MaxDeviation = 0.0;
			RunFor(Adapter, 1.0, [&]()
			{
				for (int32 Substep = 0; Substep < AdapterSubsteps; ++Substep)
				{
					Reference.Tick();
				}
				MaxDeviation = FMath::Max(MaxDeviation, FVector::Dist(Adapter.Chassis().Position, Reference.Chassis().Position));
			});

			TestTrue(FString::Printf(TEXT("max deviation from the 960 Hz reference (%g m) < 2 mm"), MaxDeviation), MaxDeviation < 2.0e-3);
		});

		It("is caught by the bump stops when dropped from 30 cm", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOHeadlessHarness Harness = MakeAdapterLikeHarness(Params, Ground, DroppedAbove(Params, Ground, 0.30));

			double MaxCompression = 0.0;
			RunFor(Harness, 2.0, [&]()
			{
				for (const FGTOWheelState& Wheel : Harness.State().Wheels)
				{
					MaxCompression = FMath::Max(MaxCompression, Wheel.Compression);
				}
			});
			const double FinalSpeed = Harness.Chassis().LinearVelocity.Size();

			TestTrue(FString::Printf(TEXT("max compression (%g m) <= 0.15 m"), MaxCompression), MaxCompression <= 0.15);
			TestTrue(FString::Printf(TEXT("settled again after 2 s (%g m/s)"), FinalSpeed), FinalSpeed < 0.01);
		});
	});
}

#endif
