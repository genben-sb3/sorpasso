#pragma once

#include "GTOVehicleParams.h"
#include "GTOVehicleState.h"

struct FGTOStepResult
{
	FGTOVehicleState State;
	FGTOChassisForces Forces;
};

struct GTOSIM_API FGTOVehicleSim
{
	/**
	 * Advances the vehicle by Dt. Pure: the result depends on the arguments only.
	 * The chassis itself is integrated by the caller (Chaos, or the headless harness) from the returned forces.
	 */
	static FGTOStepResult Step(
		const FGTOVehicleState& State,
		const FGTOVehicleParams& Params,
		const FGTOInputPacket& Packet,
		const FGTOWheelContacts& Contacts,
		const FGTOChassisKinematics& Chassis,
		double Dt);
};
