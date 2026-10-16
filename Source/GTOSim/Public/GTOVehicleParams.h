#pragma once

#include "GTOVehicleTypes.h"

/**
 * Raycast-style suspension of one axle. The strut runs from the mount straight down in body space;
 * its length is measured from the mount to the wheel centre.
 */
struct FGTOSuspensionParams
{
	double SpringRate = 0.0;          // N/m
	double BumpDamping = 0.0;         // N·s/m, while compressing
	double ReboundDamping = 0.0;      // N·s/m, while extending
	double MaxLength = 0.0;           // m, strut length at full droop (spring force is zero here)
	double BumpStopCompression = 0.0; // m, compression at which the bump stop engages
	double BumpStopRate = 0.0;        // N/m, added on top of the spring beyond that
	double AntiRollRate = 0.0;        // N/m, per metre of left/right compression difference
};

struct FGTOAxleParams
{
	double PositionX = 0.0;   // m, axle ahead of the centre of mass (rear axle negative)
	double TrackWidth = 0.0;  // m
	double MountHeight = 0.0; // m, strut mount above the centre of mass
	double WheelRadius = 0.0; // m
	FGTOSuspensionParams Suspension;
};

/** Plain vehicle data. Filled from a data asset by the Unreal adapter, or directly by tests. */
struct FGTOVehicleParams
{
	double Mass = 0.0;                    // kg
	FVector Inertia = FVector::OneVector; // kg·m², principal moments about the centre of mass in body axes
	FGTOAxleParams Front;
	FGTOAxleParams Rear;

	const FGTOAxleParams& Axle(EGTOWheel Wheel) const { return GTOIsFront(Wheel) ? Front : Rear; }

	/** Strut mount of a wheel in body space, relative to the centre of mass. */
	FVector WheelMountLocal(EGTOWheel Wheel) const
	{
		const FGTOAxleParams& A = Axle(Wheel);
		return FVector(A.PositionX, (GTOIsLeft(Wheel) ? -0.5 : 0.5) * A.TrackWidth, A.MountHeight);
	}

	/** Wheel centre in body space, relative to the centre of mass, for a given suspension compression. */
	FVector WheelCentreLocal(EGTOWheel Wheel, double Compression) const
	{
		return WheelMountLocal(Wheel) - FVector::UpVector * (Axle(Wheel).Suspension.MaxLength - Compression);
	}

	FVector WheelMountWorld(EGTOWheel Wheel, const FGTOChassisKinematics& Chassis) const
	{
		return Chassis.Position + Chassis.Orientation.RotateVector(WheelMountLocal(Wheel));
	}

	/** Height of the centre of mass above flat ground when the level car hangs on fully extended struts and the lowest wheels just touch. */
	double FullDroopHeight() const
	{
		auto CentreHeight = [](const FGTOAxleParams& Axle)
		{
			return Axle.WheelRadius + Axle.Suspension.MaxLength - Axle.MountHeight;
		};
		return FMath::Max(CentreHeight(Front), CentreHeight(Rear));
	}
};
