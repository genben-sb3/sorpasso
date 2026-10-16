#include "GTOStateInterpolation.h"

FGTOVehicleState GTOInterpolateState(const FGTOVehicleState& From, const FGTOVehicleState& To, double Alpha)
{
	FGTOVehicleState Result;
	Result.Chassis.Position = FMath::Lerp(From.Chassis.Position, To.Chassis.Position, Alpha);
	Result.Chassis.Orientation = FQuat::Slerp(From.Chassis.Orientation, To.Chassis.Orientation, Alpha);
	Result.Chassis.LinearVelocity = FMath::Lerp(From.Chassis.LinearVelocity, To.Chassis.LinearVelocity, Alpha);
	Result.Chassis.AngularVelocity = FMath::Lerp(From.Chassis.AngularVelocity, To.Chassis.AngularVelocity, Alpha);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		Result.Wheels[Index].Compression = FMath::Lerp(From.Wheels[Index].Compression, To.Wheels[Index].Compression, Alpha);
		Result.Wheels[Index].Load = FMath::Lerp(From.Wheels[Index].Load, To.Wheels[Index].Load, Alpha);
	}
	return Result;
}

FGTOSnapshotInterpolator::FGTOSnapshotInterpolator(double InStepDt)
	: StepDt(InStepDt)
{
}

void FGTOSnapshotInterpolator::Reset(const FGTOVehicleState& State, double InStepDt)
{
	Previous = Latest = State;
	StepDt = InStepDt;
	Remainder = 0.0;
}

void FGTOSnapshotInterpolator::Push(const FGTOVehicleState& State)
{
	Previous = Latest;
	Latest = State;
	Remainder -= StepDt;
}

void FGTOSnapshotInterpolator::Advance(double FrameDt)
{
	// Clamped, so a sim that falls behind the frame time holds the latest snapshot instead of extrapolating.
	Remainder = FMath::Clamp(Remainder + FrameDt, 0.0, StepDt);
}

FGTOVehicleState FGTOSnapshotInterpolator::Sample() const
{
	return GTOInterpolateState(Previous, Latest, StepDt > 0.0 ? Remainder / StepDt : 1.0);
}
