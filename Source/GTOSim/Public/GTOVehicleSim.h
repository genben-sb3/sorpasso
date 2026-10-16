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

	/**
	 * Advances the vehicle by Dt in NumSubsteps equal steps against contacts that stay fixed for the whole Dt,
	 * integrating the chassis locally under Gravity in between. Returns the state after the last substep and the
	 * forces averaged over Dt, for a caller that moves its rigid body only once per Dt (the adapter on a Chaos step).
	 */
	static FGTOStepResult StepSubstepped(
		const FGTOVehicleState& State,
		const FGTOVehicleParams& Params,
		const FGTOInputPacket& Packet,
		const FGTOWheelContacts& Contacts,
		const FGTOChassisKinematics& Chassis,
		const FVector& Gravity,
		double Dt,
		int32 NumSubsteps);
};
