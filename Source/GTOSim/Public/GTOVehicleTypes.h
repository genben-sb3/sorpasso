#pragma once

// Sim core conventions: SI units (m, kg, s, N, rad) and Unreal axes (X forward, Y right, Z up).
// The Unreal adapter converts from and to centimetres.

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

/** Wheel slots, in the order every per-wheel array uses. Left is -Y. */
enum class EGTOWheel : uint8
{
	FrontLeft,
	FrontRight,
	RearLeft,
	RearRight,
};

inline constexpr int32 GTONumWheels = 4;

inline bool GTOIsFront(EGTOWheel Wheel) { return Wheel == EGTOWheel::FrontLeft || Wheel == EGTOWheel::FrontRight; }
inline bool GTOIsLeft(EGTOWheel Wheel) { return Wheel == EGTOWheel::FrontLeft || Wheel == EGTOWheel::RearLeft; }

/** Controls for one sim tick. The same packet drives player, AI, replay and ghost. */
struct FGTOInputPacket
{
	uint32 Tick = 0;
	float Steer = 0.f;     // -1 (left) … 1 (right)
	float Throttle = 0.f;  // 0 … 1
	float Brake = 0.f;     // 0 … 1
	float Clutch = 0.f;    // 0 … 1
	float Handbrake = 0.f; // 0 … 1
	int8 GearRequest = 0;
	uint8 AidFlags = 0;
};

/** Chassis rigid-body state in world space. Position is the centre of mass. */
struct FGTOChassisKinematics
{
	FVector Position = FVector::ZeroVector;
	FQuat Orientation = FQuat::Identity;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
};

/** Ground found under one wheel. The sim treats it as a plane for the whole step. */
struct FGTOWheelContact
{
	bool bHit = false;
	FVector Point = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	double Friction = 1.0;
};

using FGTOWheelContacts = TStaticArray<FGTOWheelContact, GTONumWheels>;

/** Force and torque on the chassis in world space, torque about the centre of mass. Gravity is not included. */
struct FGTOChassisForces
{
	FVector Force = FVector::ZeroVector;
	FVector Torque = FVector::ZeroVector;
};
