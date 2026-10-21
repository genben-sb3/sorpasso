#include "Vehicle/GTOStateInterpolation.h"

FGTOVehicleState GTOInterpolateState(const FGTOVehicleState& From, const FGTOVehicleState& To, double Alpha)
{
	FGTOVehicleState Result;
	Result.Chassis.Position = FMath::Lerp(From.Chassis.Position, To.Chassis.Position, Alpha);
	Result.Chassis.Orientation = FQuat::Slerp(From.Chassis.Orientation, To.Chassis.Orientation, Alpha);
	Result.Chassis.LinearVelocity = FMath::Lerp(From.Chassis.LinearVelocity, To.Chassis.LinearVelocity, Alpha);
	Result.Chassis.AngularVelocity = FMath::Lerp(From.Chassis.AngularVelocity, To.Chassis.AngularVelocity, Alpha);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FGTOWheelState& A = From.Wheels[Index];
		const FGTOWheelState& B = To.Wheels[Index];
		FGTOWheelState& Wheel = Result.Wheels[Index];
		Wheel.Compression = FMath::Lerp(A.Compression, B.Compression, Alpha);
		Wheel.Load = FMath::Lerp(A.Load, B.Load, Alpha);
		Wheel.AngularVelocity = FMath::Lerp(A.AngularVelocity, B.AngularVelocity, Alpha);
		Wheel.SteerAngle = FMath::Lerp(A.SteerAngle, B.SteerAngle, Alpha);
		Wheel.SlipRatio = FMath::Lerp(A.SlipRatio, B.SlipRatio, Alpha);
		Wheel.SlipAngle = FMath::Lerp(A.SlipAngle, B.SlipAngle, Alpha);
		Wheel.LongitudinalForce = FMath::Lerp(A.LongitudinalForce, B.LongitudinalForce, Alpha);
		Wheel.LateralForce = FMath::Lerp(A.LateralForce, B.LateralForce, Alpha);
		Wheel.SurfaceFriction = Alpha < 0.5 ? A.SurfaceFriction : B.SurfaceFriction;
		// A plain blend would spin the wheel backwards across the wrap at ±π; the adapter blends the spin with the
		// physics package's animation data instead.
		Wheel.SpinAngle = Alpha < 0.5 ? A.SpinAngle : B.SpinAngle;
	}
	Result.Gear = Alpha < 0.5 ? From.Gear : To.Gear;
	Result.EngineSpeed = FMath::Lerp(From.EngineSpeed, To.EngineSpeed, Alpha);
	Result.SteeringWheelAngle = FMath::Lerp(From.SteeringWheelAngle, To.SteeringWheelAngle, Alpha);
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
	return GTOInterpolateState(Previous, Latest, GetAlpha());
}

double FGTOSnapshotInterpolator::GetAlpha() const
{
	return StepDt > 0.0 ? Remainder / StepDt : 1.0;
}
