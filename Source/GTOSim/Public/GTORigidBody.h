#pragma once

#include "GTOVehicleTypes.h"

/** Semi-implicit Euler step of the chassis as a free rigid body. Inertia holds the principal moments in body axes. */
GTOSIM_API FGTOChassisKinematics GTOIntegrateRigidBody(
	const FGTOChassisKinematics& Chassis,
	double Mass,
	const FVector& Inertia,
	const FGTOChassisForces& Forces,
	const FVector& Gravity,
	double Dt);
