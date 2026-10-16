#include "GTOSuspension.h"

FGTOSuspensionWheels GTOEvaluateSuspension(const FGTOVehicleParams& Params, const FGTOWheelContacts& Contacts, const FGTOChassisKinematics& Chassis)
{
	FGTOSuspensionWheels Wheels;
	const FVector StrutDirection = Chassis.Orientation.RotateVector(-FVector::UpVector);

	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FGTOWheelContact& Contact = Contacts[Index];
		if (!Contact.bHit)
		{
			continue;
		}

		const FVector Normal = Contact.Normal.GetSafeNormal();
		const double StrutDotNormal = FVector::DotProduct(StrutDirection, Normal);
		if (StrutDotNormal > -UE_KINDA_SMALL_NUMBER)
		{
			continue; // strut parallel to or pointing away from the ground
		}

		const EGTOWheel Wheel = static_cast<EGTOWheel>(Index);
		const FGTOAxleParams& Axle = Params.Axle(Wheel);
		const FGTOSuspensionParams& Suspension = Axle.Suspension;

		// Distance along the strut axis from the mount to the ground plane.
		const FVector Mount = Params.WheelMountWorld(Wheel, Chassis);
		const double DistanceToGround = FVector::DotProduct(Contact.Point - Mount, Normal) / StrutDotNormal;
		const double Compression = Suspension.MaxLength - (DistanceToGround - Axle.WheelRadius);
		if (Compression <= 0.0)
		{
			continue; // wheel hangs at full droop
		}

		// Analytic compression rate from the mount velocity, so the step needs no previous compression.
		const FVector MountVelocity = Chassis.LinearVelocity + FVector::CrossProduct(Chassis.AngularVelocity, Mount - Chassis.Position);
		const double CompressionRate = FVector::DotProduct(MountVelocity, Normal) / StrutDotNormal;
		const double Damping = CompressionRate > 0.0 ? Suspension.BumpDamping : Suspension.ReboundDamping;
		const double BumpStop = Suspension.BumpStopRate * FMath::Max(0.0, Compression - Suspension.BumpStopCompression);

		FGTOSuspensionWheel& Out = Wheels[Index];
		Out.bInContact = true;
		Out.Compression = Compression;
		Out.Load = Suspension.SpringRate * Compression + BumpStop + Damping * CompressionRate;
		Out.ContactPoint = Mount + StrutDirection * DistanceToGround;
		Out.Normal = Normal;
	}

	// Anti-roll bars push the more compressed side down harder and lift the other one; a wheel in the air counts as uncompressed.
	for (const EGTOWheel Left : { EGTOWheel::FrontLeft, EGTOWheel::RearLeft })
	{
		FGTOSuspensionWheel& LeftWheel = Wheels[static_cast<int32>(Left)];
		FGTOSuspensionWheel& RightWheel = Wheels[static_cast<int32>(Left) + 1];
		const double AntiRoll = Params.Axle(Left).Suspension.AntiRollRate * (LeftWheel.Compression - RightWheel.Compression);
		LeftWheel.Load += AntiRoll;
		RightWheel.Load -= AntiRoll;
	}

	for (FGTOSuspensionWheel& Wheel : Wheels)
	{
		Wheel.Load = Wheel.bInContact ? FMath::Max(0.0, Wheel.Load) : 0.0; // the ground can only push
	}

	return Wheels;
}
