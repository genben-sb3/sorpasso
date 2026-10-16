#include "GTOVehicleSim.h"
#include "GTORigidBody.h"
#include "GTOSuspension.h"

namespace
{
	/** |vx| as the slip denominator, saturated smoothly towards 2/3 of MinSpeed at standstill (MathWorks' VXLOW). */
	double SlipDenominator(double Vx, double MinSpeed)
	{
		const double Speed = FMath::Abs(Vx);
		if (MinSpeed <= 0.0 || Speed >= MinSpeed)
		{
			return FMath::Max(Speed, UE_KINDA_SMALL_NUMBER);
		}
		return 2.0 * MinSpeed / (3.0 - FMath::Square(Speed / MinSpeed));
	}

	/**
	 * A brake torque as a bounded impulse towards ω = 0: it can stop the wheel, but never turn it backwards. Inertia is
	 * what the brake has to slow down, the wheel alone or the wheel plus what its tyre drags along.
	 */
	double ApplyBrake(double AngularVelocity, double BrakeTorque, double Inertia, double Dt)
	{
		const double MaxChange = BrakeTorque * Dt / Inertia;
		return FMath::Abs(AngularVelocity) <= MaxChange ? 0.0 : AngularVelocity - FMath::Sign(AngularVelocity) * MaxChange;
	}

	/** Torque of the drive stand-in on one rear wheel. */
	double DriveTorque(const FGTODriveParams& Drive, double Throttle, double AngularVelocity)
	{
		const double PowerLimit = 0.5 * Drive.MaxPower / FMath::Max(FMath::Abs(AngularVelocity), UE_KINDA_SMALL_NUMBER);
		return Throttle * FMath::Min(0.5 * Drive.MaxTorque, PowerLimit);
	}

	struct FWheelStep
	{
		FGTOWheelState State;
		FVector Force = FVector::ZeroVector; // on the chassis at the contact point, world space
	};

	/**
	 * Tyre and wheel spin of one wheel over Dt. The wheel spin is stepped implicitly against the tyre's longitudinal
	 * stiffness, so the stiff wheel-tyre coupling stays stable at any slip; the brake and the rolling resistance follow
	 * as a bounded impulse.
	 */
	FWheelStep StepWheel(
		const FGTOWheelState& Before,
		const FGTOAxleParams& Axle,
		const IGTOTireModel& Tire,
		const FGTOSuspensionWheel& Suspension,
		const FGTOWheelContact& Contact,
		const FGTOChassisKinematics& Chassis,
		double SteerAngle,
		double DriveTorque,
		double BrakeTorque,
		double Dt)
	{
		FWheelStep Result;
		FGTOWheelState& Wheel = Result.State;
		Wheel.Compression = Suspension.Compression;
		Wheel.Load = Suspension.Load;
		Wheel.SteerAngle = SteerAngle;

		const double Radius = Axle.WheelRadius;
		const double Inertia = Axle.WheelInertia;
		if (!Suspension.bInContact)
		{
			Wheel.AngularVelocity = ApplyBrake(Before.AngularVelocity + DriveTorque * Dt / Inertia, BrakeTorque, Inertia, Dt);
			return Result;
		}

		// Wheel axes in the ground plane: heading along the steered wheel, right across it.
		const FVector Normal = Suspension.Normal;
		const FVector Heading = Chassis.Orientation.RotateVector(FVector(FMath::Cos(SteerAngle), FMath::Sin(SteerAngle), 0.0));
		const FVector Forward = (Heading - Normal * FVector::DotProduct(Heading, Normal)).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(Normal, Forward);
		const FVector ContactVelocity = Chassis.LinearVelocity + FVector::CrossProduct(Chassis.AngularVelocity, Suspension.ContactPoint - Chassis.Position);
		const double Vx = FVector::DotProduct(ContactVelocity, Forward);
		const double Vy = FVector::DotProduct(ContactVelocity, Right);

		const FGTOTireParams& TireParams = Axle.Tire;
		const double Denominator = SlipDenominator(Vx, TireParams.SlipMinSpeed);

		// The slip angle builds up over the relaxation length. Below RelaxationMinSpeed it acts at once: the lag would
		// turn the tyre into an undamped spring there and rock the parked car (ADR 0002). Above it the lag blends in.
		const double KinematicSlipAngle = FMath::Atan2(-Vy, Denominator);
		const double Speed = FVector2D(Vx, Vy).Size();
		const double MinSpeed = TireParams.RelaxationMinSpeed;
		Wheel.SlipAngle = KinematicSlipAngle;
		if (TireParams.RelaxationLength > 0.0 && Speed > MinSpeed)
		{
			const double Relaxed = FMath::Lerp(Before.SlipAngle, KinematicSlipAngle, 1.0 - FMath::Exp(-Speed * Dt / TireParams.RelaxationLength));
			const double FullSpeed = FMath::Max(TireParams.RelaxationFullSpeed, MinSpeed + UE_KINDA_SMALL_NUMBER);
			Wheel.SlipAngle = FMath::Lerp(KinematicSlipAngle, Relaxed, FMath::SmoothStep(MinSpeed, FullSpeed, Speed));
		}

		const auto ForcesAt = [&](double AngularVelocity)
		{
			return Tire.Evaluate((AngularVelocity * Radius - Vx) / Denominator, Wheel.SlipAngle, Suspension.Load, Contact.Friction, 0.0);
		};

		// I·dω/dt = T_drive − Fx·r − T_brake with Fx(ω) linearised; past the peak the slope counts as zero. Through the
		// slope the tyre adds to the inertia the drive and the brake work against: at small slip they move the whole car.
		const double Omega = Before.AngularVelocity;
		const double DeltaOmega = 1.0e-4 * Denominator / Radius;
		const double Stiffness = FMath::Max(0.0, (ForcesAt(Omega + DeltaOmega).Longitudinal - ForcesAt(Omega - DeltaOmega).Longitudinal) / (2.0 * DeltaOmega));
		const double EffectiveInertia = Inertia + Dt * Radius * Stiffness;
		const double Unbraked = Omega + Dt * (DriveTorque - ForcesAt(Omega).Longitudinal * Radius) / EffectiveInertia;
		const double RollingResistance = TireParams.RollingResistance * Suspension.Load * Radius;
		Wheel.AngularVelocity = ApplyBrake(Unbraked, BrakeTorque + RollingResistance, EffectiveInertia, Dt);

		const FGTOTireForces Forces = ForcesAt(Wheel.AngularVelocity);
		Wheel.SlipRatio = (Wheel.AngularVelocity * Radius - Vx) / Denominator;
		Wheel.LongitudinalForce = Forces.Longitudinal;
		Wheel.LateralForce = Forces.Lateral;
		Result.Force = Forward * Forces.Longitudinal + Right * Forces.Lateral;
		return Result;
	}
}

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

	const double Steer = FMath::Clamp(Packet.Steer, -1.f, 1.f);
	const double Throttle = FMath::Clamp(Packet.Throttle, 0.f, 1.f);
	const double Brake = FMath::Clamp(Packet.Brake, 0.f, 1.f);
	const double Handbrake = FMath::Clamp(Packet.Handbrake, 0.f, 1.f);

	const FGTOMagicFormulaTire FrontTire(Params.Front.Tire);
	const FGTOMagicFormulaTire RearTire(Params.Rear.Tire);
	const FGTOSuspensionWheels Suspension = GTOEvaluateSuspension(Params, Contacts, Chassis);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const EGTOWheel WheelSlot = static_cast<EGTOWheel>(Index);
		const bool bFront = GTOIsFront(WheelSlot);
		const FGTOAxleParams& Axle = Params.Axle(WheelSlot);
		const FGTOWheelState& Before = State.Wheels[Index];

		const double SteerAngle = Steer * Axle.MaxSteerAngle;
		const double Drive = bFront ? 0.0 : DriveTorque(Params.Drive, Throttle, Before.AngularVelocity);
		const double BrakeTorque = Brake * Axle.BrakeTorque + (bFront ? 0.0 : Handbrake * Params.HandbrakeTorque);
		const FWheelStep Wheel = StepWheel(Before, Axle, bFront ? FrontTire : RearTire, Suspension[Index], Contacts[Index], Chassis, SteerAngle, Drive, BrakeTorque, Dt);
		Result.State.Wheels[Index] = Wheel.State;

		const FGTOSuspensionWheel& Strut = Suspension[Index];
		const FVector Force = Strut.Normal * Strut.Load + Wheel.Force;
		Result.Forces.Force += Force;
		Result.Forces.Torque += FVector::CrossProduct(Strut.ContactPoint - Chassis.Position, Force);
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
