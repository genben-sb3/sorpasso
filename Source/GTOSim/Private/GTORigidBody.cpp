#include "GTORigidBody.h"

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
