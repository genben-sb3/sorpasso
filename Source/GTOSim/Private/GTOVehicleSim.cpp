#include "GTOVehicleSim.h"
#include "GTORigidBody.h"
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

FGTOStepResult FGTOVehicleSim::StepSubstepped(
	const FGTOVehicleState& State,
	const FGTOVehicleParams& Params,
	const FGTOInputPacket& Packet,
	const FGTOWheelContacts& Contacts,
	const FGTOChassisKinematics& Chassis,
	const FVector& Gravity,
	double Dt,
	int32 NumSubsteps)
{
	check(NumSubsteps > 0);
	const double SubDt = Dt / NumSubsteps;

	FGTOStepResult Result;
	Result.State = State;
	FGTOChassisKinematics Local = Chassis;
	for (int32 Substep = 0; Substep < NumSubsteps; ++Substep)
	{
		const FGTOStepResult Sub = Step(Result.State, Params, Packet, Contacts, Local, SubDt);
		Result.State = Sub.State;
		Result.Forces.Force += Sub.Forces.Force / NumSubsteps;
		Result.Forces.Torque += Sub.Forces.Torque / NumSubsteps;
		Local = GTOIntegrateRigidBody(Local, Params.Mass, Params.Inertia, Sub.Forces, Gravity, SubDt);
	}
	return Result;
}
