#include "Misc/AutomationTest.h"
#include "Vehicle/GTOStateInterpolation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double StepDt = 1.0 / 120.0; // s
	constexpr double FrameDt = 1.0 / 60.0; // s, two sim steps

	// Car at X metres, yawed by Yaw degrees, with every wheel compressed by Compression.
	FGTOVehicleState CarAt(double X, double Yaw = 0.0, double Compression = 0.0)
	{
		FGTOVehicleState State;
		State.Chassis.Position = FVector(X, 0.0, 0.5);
		State.Chassis.Orientation = FQuat(FRotator(0.0, Yaw, 0.0));
		for (FGTOWheelState& Wheel : State.Wheels)
		{
			Wheel.Compression = Compression;
		}
		return State;
	}
}

BEGIN_DEFINE_SPEC(FGTOStateInterpolationSpec, "GTO.Vehicle.Interpolation", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
	FGTOSnapshotInterpolator Display;
	int64 Steps = 0; // snapshots pushed since the reset; snapshot n stands at x = n

	// One frame as the adapter runs it: the snapshots of the frame's sim steps, then the frame time. Returns the shown X.
	double Frame(double Dt, int32 NewSteps)
	{
		for (int32 Index = 0; Index < NewSteps; ++Index)
		{
			Display.Push(CarAt(static_cast<double>(++Steps)));
		}
		Display.Advance(Dt);
		return Display.Sample().Chassis.Position.X;
	}

	// Four seconds of even 60 fps frames with two sim steps each.
	void Settle()
	{
		for (int32 Index = 0; Index < 240; ++Index)
		{
			Frame(FrameDt, 2);
		}
	}
END_DEFINE_SPEC(FGTOStateInterpolationSpec)

void FGTOStateInterpolationSpec::Define()
{
	BeforeEach([this]()
	{
		Steps = 0;
		Display.Reset(CarAt(0.0), StepDt);
	});

	It("blends position, orientation and wheels between two snapshots", [this]()
	{
		const FGTOVehicleState Shown = GTOInterpolateState(CarAt(0.0), CarAt(1.0, 90.0, 0.1), 0.25);
		TestNearlyEqual(TEXT("x a quarter of the way"), Shown.Chassis.Position.X, 0.25, 1.0e-9);
		TestNearlyEqual(TEXT("yaw a quarter of the way"), Shown.Chassis.Orientation.Rotator().Yaw, 22.5, 1.0e-6);
		TestNearlyEqual(TEXT("compression a quarter of the way"), Shown.Wheels[0].Compression, 0.025, 1.0e-9);
	});

	It("shows the start snapshot until the first sim step", [this]()
	{
		TestEqual(TEXT("x"), Frame(0.5 * StepDt, 0), 0.0);
	});

	It("holds the newest snapshot instead of extrapolating when the sim falls behind", [this]()
	{
		Frame(StepDt, 1);
		TestNearlyEqual(TEXT("x"), Frame(5.0 * StepDt, 0), 1.0, 1.0e-9);
	});

	It("settles at the set delay behind the newest snapshot", [this]()
	{
		Settle();
		TestNearlyEqual(TEXT("steps behind"), Display.GetShownDelay(), FGTOSnapshotInterpolator::Delay, 0.01);
		TestNearlyEqual(TEXT("x"), Display.Sample().Chassis.Position.X, Steps - FGTOSnapshotInterpolator::Delay, 0.01);
	});

	It("names the snapshots it blends, so data kept alongside them blends the same way", [this]()
	{
		Settle();
		Frame(0.3 * StepDt, 0); // between two snapshots
		const FGTOSnapshotInterpolator::FBlend Blend = Display.GetBlend();
		TestEqual(TEXT("neighbours"), Blend.To, Blend.From + 1);
		TestTrue(TEXT("between them"), Blend.Alpha > 0.0 && Blend.Alpha < 1.0);
		TestNearlyEqual(TEXT("x"), Display.Sample().Chassis.Position.X, Blend.From + Blend.Alpha, 1.0e-9);
	});

	It("moves with frame time when frame times vary", [this]()
	{
		Settle();
		// Chaos steps on its own accumulator, which carries the rest of each frame over to the next.
		const double FrameTimes[] = { 0.014, 0.019, 0.016, 0.0175, 0.015, 0.018 }; // s
		double Accumulated = 0.0; // s
		double Before = Display.Sample().Chassis.Position.X;
		for (int32 Index = 0; Index < 60; ++Index)
		{
			const double Dt = FrameTimes[Index % UE_ARRAY_COUNT(FrameTimes)];
			Accumulated += Dt;
			const int32 NewSteps = FMath::FloorToInt32(Accumulated / StepDt);
			Accumulated -= NewSteps * StepDt;
			const double Shown = Frame(Dt, NewSteps);
			TestNearlyEqual(*FString::Printf(TEXT("step of frame %d"), Index), Shown - Before, Dt / StepDt, 0.1);
			Before = Shown;
		}
	});

	// The stutter on screen: Chaos hands the steps of a frame over a frame late now and then, so one frame brings none and
	// the next four. Counting arrivals made the car stop for a frame and then jump.
	It("moves evenly when a frame's sim steps arrive a frame late", [this]()
	{
		Settle();
		double Before = Display.Sample().Chassis.Position.X;
		for (int32 Index = 0; Index < 60; ++Index)
		{
			const int32 NewSteps = Index % 5 == 0 ? 0 : (Index % 5 == 1 ? 4 : 2);
			const double Shown = Frame(FrameDt, NewSteps);
			TestNearlyEqual(*FString::Printf(TEXT("step of frame %d"), Index), Shown - Before, FrameDt / StepDt, 0.1);
			Before = Shown;
		}
	});

	It("jumps to the set delay after a hitch instead of crawling through old snapshots", [this]()
	{
		Settle();
		const double Shown = Frame(FrameDt, 12); // the sim caught up on a 0.1 s hitch
		TestNearlyEqual(TEXT("x"), Shown, Steps - FGTOSnapshotInterpolator::Delay, 1.0e-9);
	});
}

#endif
