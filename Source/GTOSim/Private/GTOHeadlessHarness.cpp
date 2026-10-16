#include "GTOHeadlessHarness.h"

double FGTOFlatGround::HeightAt(const FVector2D& Location) const
{
	double Result = Height;
	for (const FGTOGroundBlock& Block : Blocks)
	{
		if (Location.X >= Block.Min.X && Location.X <= Block.Max.X && Location.Y >= Block.Min.Y && Location.Y <= Block.Max.Y)
		{
			Result = FMath::Max(Result, Block.Height);
		}
	}
	return Result;
}

FGTOWheelContacts FGTOFlatGround::Contacts(const FGTOVehicleParams& Params, const FGTOChassisKinematics& Chassis) const
{
	FGTOWheelContacts Result;
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FVector Mount = Params.WheelMountWorld(static_cast<EGTOWheel>(Index), Chassis);
		FGTOWheelContact& Contact = Result[Index];
		Contact.bHit = true;
		Contact.Point = FVector(Mount.X, Mount.Y, HeightAt(FVector2D(Mount)));
		Contact.Normal = FVector::UpVector;
		Contact.Friction = Friction;
	}
	return Result;
}

FGTOHeadlessHarness::FGTOHeadlessHarness(const FGTOVehicleParams& InParams, const FGTOFlatGround& InGround, const FGTOChassisKinematics& Start, double InDt, int32 InNumSubsteps)
	: Params(InParams)
	, Ground(InGround)
	, Body(Start)
	, StepDt(InDt)
	, NumSubsteps(InNumSubsteps)
{
	VehicleState.Chassis = Start;
}

void FGTOHeadlessHarness::Tick(const FGTOInputPacket& Packet)
{
	const FGTOWheelContacts Contacts = Ground.Contacts(Params, Body);
	const FVector GravityVector(0.0, 0.0, -Gravity);
	const FGTOStepResult Result = FGTOVehicleSim::StepSubstepped(VehicleState, Params, Packet, Contacts, Body, GravityVector, StepDt, NumSubsteps);
	VehicleState = Result.State;
	Body = GTOIntegrateRigidBody(Body, Params.Mass, Params.Inertia, Result.Forces, GravityVector, StepDt);
}
