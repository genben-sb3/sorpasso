#include "GTOVehicleSim.h"
#include "GTOSuspension.h"

FGTOStepResult FGTOVehicleSim::Step(
	const FGTOVehicleState& State,
	const FGTOVehicleParams& Params,
	const FGTOInputPacket& Packet,
	const FGTOWheelContacts& Contacts,
	const FGTOChassisKinematics& Chassis,
	double Dt)
{
	FGTOStepResult Result;
	Result.State.Chassis = Chassis;

	const FGTOSuspensionWheels Suspension = GTOEvaluateSuspension(Params, Contacts, Chassis);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FGTOSuspensionWheel& Wheel = Suspension[Index];
		Result.State.Wheels[Index].Compression = Wheel.Compression;
		Result.State.Wheels[Index].Load = Wheel.Load;

		const FVector Force = Wheel.Normal * Wheel.Load;
		Result.Forces.Force += Force;
		Result.Forces.Torque += FVector::CrossProduct(Wheel.ContactPoint - Chassis.Position, Force);
	}

	return Result;
}
