#pragma once

#include "GTOVehicleTypes.h"

struct FGTOWheelState
{
	double Compression = 0.0;       // m, 0 at full droop or in the air
	double Load = 0.0;              // N, along the contact normal, never negative
	double AngularVelocity = 0.0;   // rad/s, positive rolling forward
	double SteerAngle = 0.0;        // rad, positive to the right
	double SlipRatio = 0.0;         // (ωr − vx)/|vx|, positive when driving
	double SlipAngle = 0.0;         // rad, as the tyre feels it after relaxation; positive when the tyre is pushed right
	double LongitudinalForce = 0.0; // N, tyre force along the wheel heading
	double LateralForce = 0.0;      // N, tyre force to the right of the wheel heading
};

/** Complete, copyable vehicle state. Serves as the snapshot for replay, ghost, display and tests. */
struct FGTOVehicleState
{
	FGTOChassisKinematics Chassis; // the kinematics the last step was computed from
	TStaticArray<FGTOWheelState, GTONumWheels> Wheels;
};
