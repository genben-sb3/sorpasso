#include "Misc/AutomationTest.h"
#include "Vehicle/GTOStateInterpolation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double StepDt = 1.0 / 120.0;

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

	// One frame as the adapter runs it: the snapshots of the frame's sim steps, then the frame time. Returns the shown X.
	double Frame(double FrameDt, std::initializer_list<FGTOVehicleState> Snapshots = {})
	{
		for (const FGTOVehicleState& Snapshot : Snapshots)
		{
			Display.Push(Snapshot);
		}
		Display.Advance(FrameDt);
		return Display.Sample().Chassis.Position.X;
	}
END_DEFINE_SPEC(FGTOStateInterpolationSpec)

void FGTOStateInterpolationSpec::Define()
{
	BeforeEach([this]()
	{
		Display.Reset(CarAt(0.0), StepDt);
	});

	It("shows the start snapshot until the first sim step", [this]()
	{
		TestEqual(TEXT("x"), Frame(0.5 * StepDt), 0.0);
	});

	It("runs one sim step behind the newest snapshot and blends towards it by frame time", [this]()
	{
		TestEqual(TEXT("x when the step arrives"), Frame(StepDt, { CarAt(1.0, 90.0, 0.1) }), 0.0);

		Frame(0.25 * StepDt);
		const FGTOVehicleState Shown = Display.Sample();
		TestNearlyEqual(TEXT("x a quarter of the way"), Shown.Chassis.Position.X, 0.25, 1.0e-9);
		TestNearlyEqual(TEXT("yaw a quarter of the way"), Shown.Chassis.Orientation.Rotator().Yaw, 22.5, 1.0e-6);
		TestNearlyEqual(TEXT("compression a quarter of the way"), Shown.Wheels[0].Compression, 0.025, 1.0e-9);
	});

	It("moves in even steps when frames come twice as fast as sim steps", [this]()
	{
		Frame(0.5 * StepDt);
		Frame(0.5 * StepDt, { CarAt(1.0) });
		TestNearlyEqual(TEXT("frame 3"), Frame(0.5 * StepDt), 0.5, 1.0e-9);
		TestNearlyEqual(TEXT("frame 4"), Frame(0.5 * StepDt, { CarAt(2.0) }), 1.0, 1.0e-9);
		TestNearlyEqual(TEXT("frame 5"), Frame(0.5 * StepDt), 1.5, 1.0e-9);
	});

	It("blends between the newest two when a long frame brings several sim steps", [this]()
	{
		TestNearlyEqual(TEXT("x after the long frame"), Frame(3.0 * StepDt, { CarAt(1.0), CarAt(2.0), CarAt(3.0) }), 2.0, 1.0e-9);
		TestNearlyEqual(TEXT("x half a step later"), Frame(0.5 * StepDt), 2.5, 1.0e-9);
	});

	It("holds the newest snapshot instead of extrapolating when the sim falls behind", [this]()
	{
		Frame(StepDt, { CarAt(1.0) });
		TestNearlyEqual(TEXT("x"), Frame(5.0 * StepDt), 1.0, 1.0e-9);
	});
}

#endif
