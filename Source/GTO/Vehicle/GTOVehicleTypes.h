#pragma once

// Game-side view of a car: SI units (m, kg, s, N, rad) with Unreal axes (X forward, Y right, Z up), like the track and
// the lap timer. The physics package works in ISO 8855 (Y left); only the adapter, UGTOVehicleSimComponent, converts.

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

/** Wheel slots, in the order every per-wheel array uses; the same order as the physics package. Left is -Y. */
enum class EGTOWheel : uint8
{
	FrontLeft,
	FrontRight,
	RearLeft,
	RearRight,
};

inline constexpr int32 GTONumWheels = 4;

inline bool GTOIsFront(EGTOWheel Wheel) { return Wheel == EGTOWheel::FrontLeft || Wheel == EGTOWheel::FrontRight; }
inline bool GTOIsLeft(EGTOWheel Wheel) { return Wheel == EGTOWheel::FrontLeft || Wheel == EGTOWheel::RearLeft; }

/** What the driver asks for on one frame, before the adapter turns it into the physics package's input packet. */
struct FGTODriverInput
{
	float Steer = 0.f;         // -1 (left) … 1 (right)
	float Throttle = 0.f;      // 0 … 1
	float Brake = 0.f;         // 0 … 1
	float Handbrake = 0.f;     // 0 … 1
	int8 GearRequest = 1;      // -1 reverse, 0 neutral, 1 drive (the automatic picks the forward gear)
	int32 TractionControl = 0; // level 0 (off) … 5
	bool bHuman = false;       // from keyboard or gamepad: goes through the input smoothing; test drivers do not
};

/** Chassis rigid-body state in world space. Position is the centre of mass. */
struct FGTOChassisKinematics
{
	FVector Position = FVector::ZeroVector;        // m
	FQuat Orientation = FQuat::Identity;
	FVector LinearVelocity = FVector::ZeroVector;  // m/s
	FVector AngularVelocity = FVector::ZeroVector; // rad/s
};

struct FGTOWheelState
{
	double Compression = 0.0;       // m, 0 at full droop or in the air
	double Load = 0.0;              // N, along the contact normal, never negative
	double AngularVelocity = 0.0;   // rad/s, positive rolling forward
	double SpinAngle = 0.0;         // rad, how far the wheel has turned, positive rolling forward, in [-π, π)
	double SteerAngle = 0.0;        // rad, positive to the right
	double SlipRatio = 0.0;         // positive when driving
	double SlipAngle = 0.0;         // rad, as the tyre feels it after relaxation; positive when the tyre is pushed right
	double LongitudinalForce = 0.0; // N, tyre force along the wheel heading
	double LateralForce = 0.0;      // N, tyre force to the right of the wheel heading
	double SurfaceFriction = 0.0;   // grip of the ground under the wheel, 1 for dry asphalt; 0 in the air
};

/** Copyable snapshot of a car for display, HUD and tests. */
struct FGTOVehicleState
{
	FGTOChassisKinematics Chassis; // the kinematics the last step was computed from
	TStaticArray<FGTOWheelState, GTONumWheels> Wheels;
	int32 Gear = 0;                  // -1 reverse, 0 neutral, 1 … n forward gears
	double EngineSpeed = 0.0;        // rad/s
	double SteeringWheelAngle = 0.0; // rad, rotation of the steering wheel, positive to the right
};
