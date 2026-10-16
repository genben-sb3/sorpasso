#pragma once

#include "GTOVehicleParams.h"

struct FGTOSuspensionWheel
{
	bool bInContact = false;
	double Compression = 0.0;          // m
	double Load = 0.0;                 // N, along Normal, never negative
	FVector ContactPoint = FVector::ZeroVector; // where the strut axis meets the ground plane
	FVector Normal = FVector::UpVector;
};

using FGTOSuspensionWheels = TStaticArray<FGTOSuspensionWheel, GTONumWheels>;

/** Spring, damper, bump stop and anti-roll bar of all four struts against the given ground contacts. */
FGTOSuspensionWheels GTOEvaluateSuspension(const FGTOVehicleParams& Params, const FGTOWheelContacts& Contacts, const FGTOChassisKinematics& Chassis);
