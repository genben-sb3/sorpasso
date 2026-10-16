#pragma once

#include "GTOVehicleTypes.h"

struct FGTOWheelState
{
	double Compression = 0.0; // m, 0 at full droop or in the air
	double Load = 0.0;        // N, along the contact normal, never negative
};

/** Complete, copyable vehicle state. Serves as the snapshot for replay, ghost, display and tests. */
struct FGTOVehicleState
{
	FGTOChassisKinematics Chassis; // the kinematics the last step was computed from
	TStaticArray<FGTOWheelState, GTONumWheels> Wheels;
};
