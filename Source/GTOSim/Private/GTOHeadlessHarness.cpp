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

FGTOChassisKinematics GTOIntegrateRigidBody(
	const FGTOChassisKinematics& Chassis,
	double Mass,
	const FVector& Inertia,
	const FGTOChassisForces& Forces,
	const FVector& Gravity,
	double Dt)
{
	FGTOChassisKinematics Result = Chassis;

	Result.LinearVelocity += (Forces.Force / Mass + Gravity) * Dt;
	Result.Position += Result.LinearVelocity * Dt;

	// Euler's equations in body axes, where the inertia tensor is diagonal.
	const FVector OmegaBody = Chassis.Orientation.UnrotateVector(Chassis.AngularVelocity);
	const FVector TorqueBody = Chassis.Orientation.UnrotateVector(Forces.Torque);
	const FVector AngularAcceleration = (TorqueBody - FVector::CrossProduct(OmegaBody, Inertia * OmegaBody)) / Inertia;
	Result.AngularVelocity = Chassis.Orientation.RotateVector(OmegaBody + AngularAcceleration * Dt);

	const double Angle = Result.AngularVelocity.Size() * Dt;
	if (Angle > 0.0)
	{
		Result.Orientation = (FQuat(Result.AngularVelocity.GetUnsafeNormal(), Angle) * Chassis.Orientation).GetNormalized();
	}
	return Result;
}

FGTOHeadlessHarness::FGTOHeadlessHarness(const FGTOVehicleParams& InParams, const FGTOFlatGround& InGround, const FGTOChassisKinematics& Start, double InDt)
	: Params(InParams)
	, Ground(InGround)
	, Body(Start)
	, StepDt(InDt)
{
	VehicleState.Chassis = Start;
}

void FGTOHeadlessHarness::Tick(const FGTOInputPacket& Packet)
{
	const FGTOWheelContacts Contacts = Ground.Contacts(Params, Body);
	const FGTOStepResult Result = FGTOVehicleSim::Step(VehicleState, Params, Packet, Contacts, Body, StepDt);
	VehicleState = Result.State;
	Body = GTOIntegrateRigidBody(Body, Params.Mass, Params.Inertia, Result.Forces, FVector(0.0, 0.0, -Gravity), StepDt);
}
