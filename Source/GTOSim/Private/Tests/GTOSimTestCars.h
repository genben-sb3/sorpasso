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

	// Road tyre at the spec's starting values: peak friction about 1.1, peak slip 0.1 and about 7 degrees.
	inline FGTOTireParams MakeRoadTire()
	{
		FGTOTireParams Tire;
		Tire.NominalLoad = 3200.0;
		Tire.Longitudinal.PeakFriction = 1.1;
		Tire.Longitudinal.LoadSensitivity = -0.15;
		Tire.Longitudinal.PeakSlip = 0.1;
		Tire.Longitudinal.Shape = 1.65;
		Tire.Longitudinal.Curvature = 0.5;
		Tire.Lateral.PeakFriction = 1.05;
		Tire.Lateral.LoadSensitivity = -0.15;
		Tire.Lateral.PeakSlip = 0.12;
		Tire.Lateral.PeakSlipLoadSensitivity = 0.25;
		Tire.Lateral.Shape = 1.3;
		Tire.Lateral.Curvature = -0.5;
		Tire.RelaxationLength = 0.4;
		Tire.RelaxationMinSpeed = 5.0;
		Tire.RelaxationFullSpeed = 10.0;
		Tire.SlipMinSpeed = 2.0;
		return Tire;
	}

	inline FGTOVehicleParams MakeSportsCar()
	{
		FGTOVehicleParams Params;
		Params.Mass = 1300.0;
		Params.Inertia = FVector(500.0, 2000.0, 2200.0);

		FGTOAxleParams Axle;
		Axle.TrackWidth = 1.55;
		Axle.WheelInertia = 1.2;
		Axle.Tire = MakeRoadTire();
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
		Params.Front.MaxSteerAngle = 0.55;
		Params.Front.BrakeTorque = 2500.0;

		Params.Rear = Axle;
		Params.Rear.PositionX = -1.125;
		Params.Rear.Suspension.SpringRate = 50000.0;
		Params.Rear.Suspension.BumpDamping = 2800.0;
		Params.Rear.Suspension.ReboundDamping = 5000.0;
		Params.Rear.Suspension.AntiRollRate = 10000.0;
		Params.Rear.BrakeTorque = 1500.0;

		Params.Drive.MaxTorque = 4500.0;
		Params.Drive.MaxPower = 210000.0;
		Params.HandbrakeTorque = 2500.0;
		return Params;
	}

	// Level chassis, at rest, with the lowest wheel hanging DropHeight above the ground at full droop.
	inline FGTOChassisKinematics DroppedAbove(const FGTOVehicleParams& Params, const FGTOFlatGround& Ground, double DropHeight)
	{
		FGTOChassisKinematics Chassis;
		Chassis.Position.Z = Ground.Height + DropHeight + Params.FullDroopHeight();
		return Chassis;
	}

	// How the Unreal adapter drives the sim core: one Chaos step of 1/120 s, 8 substeps against contacts found once
	// per step, and the chassis moved by Chaos from the forces averaged over the step.
	inline constexpr double ChaosStep = 1.0 / 120.0;
	inline constexpr int32 AdapterSubsteps = 8;

	inline FGTOHeadlessHarness MakeAdapterLikeHarness(const FGTOVehicleParams& Params, const FGTOFlatGround& Ground, const FGTOChassisKinematics& Start)
	{
		return FGTOHeadlessHarness(Params, Ground, Start, ChaosStep, AdapterSubsteps);
	}

	inline void RunFor(FGTOHeadlessHarness& Harness, double Seconds, const FGTOInputPacket& Packet, TFunctionRef<void()> AfterTick = [] {})
	{
		const int32 Ticks = FMath::RoundToInt32(Seconds / Harness.Dt());
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			Harness.Tick(Packet);
			AfterTick();
		}
	}

	inline void RunFor(FGTOHeadlessHarness& Harness, double Seconds, TFunctionRef<void()> AfterTick = [] {})
	{
		RunFor(Harness, Seconds, FGTOInputPacket(), AfterTick);
	}

	/** Chassis velocity in body axes: X forward, Y right. */
	inline FVector BodyVelocity(const FGTOChassisKinematics& Chassis)
	{
		return Chassis.Orientation.UnrotateVector(Chassis.LinearVelocity);
	}
}
