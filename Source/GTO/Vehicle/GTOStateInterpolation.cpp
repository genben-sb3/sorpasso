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
	for (FGTOVehicleState& Snapshot : Snapshots)
	{
		Snapshot = State;
	}
	StepDt = InStepDt;
	Newest = 0;
	Shown = 0.0;
}

void FGTOSnapshotInterpolator::Push(const FGTOVehicleState& State)
{
	++Newest;
	Snapshots[Slot(Newest)] = State;
}

void FGTOSnapshotInterpolator::Advance(double FrameDt)
{
	if (StepDt <= 0.0)
	{
		Shown = static_cast<double>(Newest);
		return;
	}
	Shown += FrameDt / StepDt;
	const double Error = (static_cast<double>(Newest) - Delay) - Shown; // sim steps, positive when the display lags too far
	if (FMath::Abs(Error) > Capacity - 1)
	{
		Shown += Error; // after a reset or a hitch: jump instead of crawling through stale snapshots
	}
	else
	{
		Shown += Error * FMath::Min(1.0, FrameDt / CorrectionTime);
	}
	// Never ahead of the newest snapshot: a sim that falls behind holds it instead of extrapolating.
	Shown = ShownTime();
}

double FGTOSnapshotInterpolator::ShownTime() const
{
	const double Oldest = static_cast<double>(FMath::Max<int64>(0, Newest - (Capacity - 1))); // sim steps
	return FMath::Clamp(Shown, Oldest, static_cast<double>(Newest));
}

FGTOSnapshotInterpolator::FBlend FGTOSnapshotInterpolator::GetBlend() const
{
	const double Time = ShownTime(); // sim steps
	FBlend Blend;
	Blend.From = static_cast<int64>(FMath::FloorToDouble(Time));
	Blend.To = FMath::Min(Blend.From + 1, Newest);
	Blend.Alpha = Blend.To > Blend.From ? Time - static_cast<double>(Blend.From) : 0.0;
	return Blend;
}

FGTOVehicleState FGTOSnapshotInterpolator::Sample() const
{
	const FBlend Blend = GetBlend();
	return GTOInterpolateState(Snapshots[Slot(Blend.From)], Snapshots[Slot(Blend.To)], Blend.Alpha);
}
