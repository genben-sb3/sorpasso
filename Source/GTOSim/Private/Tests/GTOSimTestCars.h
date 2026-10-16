#pragma once

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "GTOHeadlessHarness.h"

// Shared test fixtures of the sim core specs.
namespace GTOSimTest
{
	// Light rear-drive sports car. The centre of mass sits 1.375 m behind the front axle and 1.125 m ahead
	// of the rear axle, so statics put 1.125 / 2.5 = 45 % of the weight on the front axle.
	inline constexpr double FrontWeightShare = 0.45;

	inline constexpr int32 FrontLeft = static_cast<int32>(EGTOWheel::FrontLeft);
	inline constexpr int32 FrontRight = static_cast<int32>(EGTOWheel::FrontRight);
	inline constexpr int32 RearLeft = static_cast<int32>(EGTOWheel::RearLeft);
	inline constexpr int32 RearRight = static_cast<int32>(EGTOWheel::RearRight);

	using FWheelStates = TStaticArray<FGTOWheelState, GTONumWheels>;

	inline bool AllWheelsDown(const FWheelStates& Wheels)
	{
		return Algo::AllOf(Wheels, [](const FGTOWheelState& Wheel) { return Wheel.Load > 0.0; });
	}

	inline bool AnyWheelUp(const FWheelStates& Wheels)
	{
		return Algo::AnyOf(Wheels, [](const FGTOWheelState& Wheel) { return Wheel.Load == 0.0; });
	}

	inline FGTOVehicleParams MakeSportsCar()
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
	inline FGTOChassisKinematics DroppedAbove(const FGTOVehicleParams& Params, const FGTOFlatGround& Ground, double DropHeight)
	{
		FGTOChassisKinematics Chassis;
		Chassis.Position.Z = Ground.Height + DropHeight + Params.FullDroopHeight();
		return Chassis;
	}

	inline void RunFor(FGTOHeadlessHarness& Harness, double Seconds, TFunctionRef<void()> AfterTick = [] {})
	{
		const int32 Ticks = FMath::RoundToInt32(Seconds / Harness.Dt());
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Harness.Tick();
			AfterTick();
		}
	}
}
