#include "Misc/AutomationTest.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "GTOHeadlessHarness.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Light rear-drive sports car. The centre of mass sits 1.375 m behind the front axle and 1.125 m ahead
	// of the rear axle, so statics put 1.125 / 2.5 = 45 % of the weight on the front axle.
	constexpr double FrontWeightShare = 0.45;

	constexpr int32 FrontLeft = static_cast<int32>(EGTOWheel::FrontLeft);
	constexpr int32 FrontRight = static_cast<int32>(EGTOWheel::FrontRight);
	constexpr int32 RearLeft = static_cast<int32>(EGTOWheel::RearLeft);
	constexpr int32 RearRight = static_cast<int32>(EGTOWheel::RearRight);

	using FWheelStates = TStaticArray<FGTOWheelState, GTONumWheels>;

	bool AllWheelsDown(const FWheelStates& Wheels)
	{
		return Algo::AllOf(Wheels, [](const FGTOWheelState& Wheel) { return Wheel.Load > 0.0; });
	}

	bool AnyWheelUp(const FWheelStates& Wheels)
	{
		return Algo::AnyOf(Wheels, [](const FGTOWheelState& Wheel) { return Wheel.Load == 0.0; });
	}

	FGTOVehicleParams MakeSportsCar()
	{
		FGTOVehicleParams Params;
		Params.Mass = 1300.0;
		Params.Inertia = FVector(500.0, 2000.0, 2200.0);

		FGTOAxleParams Axle;
		Axle.TrackWidth = 1.55;
		Axle.MountHeight = 0.12;
		Axle.WheelRadius = 0.33;
		Axle.Suspension.MaxLength = 0.35;
		Axle.Suspension.BumpStopCompression = 0.10;
		Axle.Suspension.BumpStopRate = 5.0e5;

		Params.Front = Axle;
		Params.Front.PositionX = 1.375;
		Params.Front.Suspension.SpringRate = 45000.0;
		Params.Front.Suspension.BumpDamping = 2500.0;
		Params.Front.Suspension.ReboundDamping = 4500.0;
		Params.Front.Suspension.AntiRollRate = 20000.0;

		Params.Rear = Axle;
		Params.Rear.PositionX = -1.125;
		Params.Rear.Suspension.SpringRate = 50000.0;
		Params.Rear.Suspension.BumpDamping = 2800.0;
		Params.Rear.Suspension.ReboundDamping = 5000.0;
		Params.Rear.Suspension.AntiRollRate = 10000.0;
		return Params;
	}

	// Level chassis, at rest, with the lowest wheel hanging DropHeight above the ground at full droop.
	FGTOChassisKinematics DroppedAbove(const FGTOVehicleParams& Params, const FGTOFlatGround& Ground, double DropHeight)
	{
		auto CentreHeight = [](const FGTOAxleParams& Axle)
		{
			return Axle.WheelRadius + Axle.Suspension.MaxLength - Axle.MountHeight;
		};
		FGTOChassisKinematics Chassis;
		Chassis.Position.Z = Ground.Height + DropHeight + FMath::Max(CentreHeight(Params.Front), CentreHeight(Params.Rear));
		return Chassis;
	}

	void RunFor(FGTOHeadlessHarness& Harness, double Seconds, TFunctionRef<void()> AfterTick = [] {})
	{
		const int32 Ticks = FMath::RoundToInt32(Seconds / Harness.Dt());
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Harness.Tick();
			AfterTick();
		}
	}
}

BEGIN_DEFINE_SPEC(FGTOSuspensionSpec, "GTO.Sim.Suspension", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOSuspensionSpec)

void FGTOSuspensionSpec::Define()
{
	Describe("On flat ground", [this]()
	{
		It("comes to rest without bouncing, jittering or drifting sideways", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOChassisKinematics Start = DroppedAbove(Params, Ground, 0.05);
			Start.Orientation = FQuat(FRotator(0.5, 0.0, 1.0)); // slightly pitched and rolled, so every mode has to settle
			FGTOHeadlessHarness Harness(Params, Ground, Start);

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
			TestTrue(TEXT("chassis stands on its wheels"), Harness.Chassis().Position.Z > Ground.Height + 0.3);
			TestTrue(FString::Printf(TEXT("max speed after 2 s (%g m/s) < 1 mm/s"), MaxSpeed), MaxSpeed < 1.0e-3);
			TestTrue(FString::Printf(TEXT("max angular speed after 2 s (%g rad/s) < 1 mrad/s"), MaxAngularSpeed), MaxAngularSpeed < 1.0e-3);
			TestTrue(TEXT("no movement after 2 s"), FVector::Dist(SettledPosition, Harness.Chassis().Position) < 1.0e-3);
			TestTrue(TEXT("no horizontal drift"), FVector2D(Harness.Chassis().Position - Start.Position).Size() < 1.0e-3);
		});

		It("carries the static axle loads where the centre of mass sits", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOHeadlessHarness Harness(Params, Ground, DroppedAbove(Params, Ground, 0.05));

			RunFor(Harness, 3.0);

			const FWheelStates& Wheels = Harness.State().Wheels;
			const double Weight = Params.Mass * FGTOHeadlessHarness::Gravity;
			const double FrontLoad = Wheels[FrontLeft].Load + Wheels[FrontRight].Load;
			const double RearLoad = Wheels[RearLeft].Load + Wheels[RearRight].Load;

			// Tolerance: 0.5 % of the weight; the small pitch at rest shifts the contact points by under a millimetre.
			TestNearlyEqual(TEXT("front axle load"), FrontLoad, FrontWeightShare * Weight, 0.005 * Weight);
			TestNearlyEqual(TEXT("rear axle load"), RearLoad, (1.0 - FrontWeightShare) * Weight, 0.005 * Weight);
			TestNearlyEqual(TEXT("front left equals front right"), Wheels[FrontLeft].Load, Wheels[FrontRight].Load, 1.0);
			TestNearlyEqual(TEXT("rear left equals rear right"), Wheels[RearLeft].Load, Wheels[RearRight].Load, 1.0);
		});
	});

	Describe("With the front left wheel on a 3 cm block", [this]()
	{
		It("shifts more load onto the raised wheel's diagonal with anti-roll bars than without", [this]()
		{
			FGTOFlatGround Ground;
			Ground.Blocks.Add({ FVector2D(1.0, -1.2), FVector2D(1.8, -0.4), 0.03 });

			auto SettledLoads = [&Ground](const FGTOVehicleParams& Params)
			{
				FGTOHeadlessHarness Harness(Params, Ground, DroppedAbove(Params, Ground, 0.05));
				RunFor(Harness, 3.0);
				return Harness.State().Wheels;
			};

			const FGTOVehicleParams WithBars = MakeSportsCar();
			FGTOVehicleParams WithoutBars = WithBars;
			WithoutBars.Front.Suspension.AntiRollRate = 0.0;
			WithoutBars.Rear.Suspension.AntiRollRate = 0.0;

			const FWheelStates Bars = SettledLoads(WithBars);
			const FWheelStates NoBars = SettledLoads(WithoutBars);
			const double FrontSplitBars = Bars[FrontLeft].Load - Bars[FrontRight].Load;
			const double FrontSplitNoBars = NoBars[FrontLeft].Load - NoBars[FrontRight].Load;

			// Rigid-chassis statics with these rates predict ~1150 N with bars and ~710 N without.
			TestTrue(FString::Printf(TEXT("raised wheel carries more than its partner without bars (%g N)"), FrontSplitNoBars), FrontSplitNoBars > 0.0);
			TestTrue(FString::Printf(TEXT("front left/right split with bars (%g N) at least 30 %% above without (%g N)"), FrontSplitBars, FrontSplitNoBars),
				FrontSplitBars > 1.3 * FrontSplitNoBars);
			TestTrue(TEXT("diagonal front left + rear right carries more than front right + rear left"),
				Bars[FrontLeft].Load + Bars[RearRight].Load > Bars[FrontRight].Load + Bars[RearLeft].Load);
		});
	});

	Describe("Dropped from 30 cm", [this]()
	{
		It("is caught by the bump stops within 5 cm of where they engage", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOHeadlessHarness Harness(Params, Ground, DroppedAbove(Params, Ground, 0.30));

			double MaxCompression = 0.0;
			RunFor(Harness, 2.0, [&]()
			{
				for (const FGTOWheelState& Wheel : Harness.State().Wheels)
				{
					MaxCompression = FMath::Max(MaxCompression, Wheel.Compression);
				}
			});

			TestTrue(FString::Printf(TEXT("max compression (%g m) <= 0.15 m"), MaxCompression), MaxCompression <= 0.15);
		});
	});

	Describe("Thrown upwards at ride height, as over a crest", [this]()
	{
		It("never pulls the car towards the ground", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const FGTOFlatGround Ground;
			FGTOChassisKinematics Start = DroppedAbove(Params, Ground, -0.06); // springs 6 cm compressed
			Start.LinearVelocity.Z = 1.5;
			FGTOHeadlessHarness Harness(Params, Ground, Start);

			double MinLoad = TNumericLimits<double>::Max();
			bool bAirborne = false;
			RunFor(Harness, 2.0, [&]()
			{
				for (const FGTOWheelState& Wheel : Harness.State().Wheels)
				{
					MinLoad = FMath::Min(MinLoad, Wheel.Load);
				}
				bAirborne |= Algo::AllOf(Harness.State().Wheels, [](const FGTOWheelState& Wheel) { return Wheel.Load == 0.0; });
			});

			TestTrue(TEXT("car leaves the ground"), bAirborne);
			TestTrue(FString::Printf(TEXT("lowest wheel load (%g N) >= 0"), MinLoad), MinLoad >= 0.0);
		});
	});
}

#endif
