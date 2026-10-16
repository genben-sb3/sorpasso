#pragma once

#include "GTOVehicleSim.h"

/** Axis-aligned raised area on the harness ground, e.g. a block under one wheel. */
struct FGTOGroundBlock
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	double Height = 0.0; // m, absolute top of the block
};

/** Horizontal ground with optional raised blocks: the contact source of the headless harness. */
struct GTOSIM_API FGTOFlatGround
{
	double Height = 0.0;
	double Friction = 1.0;
	TArray<FGTOGroundBlock> Blocks;

	double HeightAt(const FVector2D& Location) const;
	FGTOWheelContacts Contacts(const FGTOVehicleParams& Params, const FGTOChassisKinematics& Chassis) const;
};

/** Semi-implicit Euler step of the chassis as a free rigid body. */
GTOSIM_API FGTOChassisKinematics GTOIntegrateRigidBody(
	const FGTOChassisKinematics& Chassis,
	double Mass,
	const FVector& Inertia,
	const FGTOChassisForces& Forces,
	const FVector& Gravity,
	double Dt);

/** Runs the sim core over many ticks without an Unreal world: flat ground in, chassis integrated here. */
class GTOSIM_API FGTOHeadlessHarness
{
public:
	static constexpr double Gravity = 9.81; // m/s²

	FGTOHeadlessHarness(const FGTOVehicleParams& InParams, const FGTOFlatGround& InGround, const FGTOChassisKinematics& Start, double InDt = 1.0 / 960.0);

	void Tick(const FGTOInputPacket& Packet = FGTOInputPacket());

	const FGTOChassisKinematics& Chassis() const { return Body; }
	const FGTOVehicleState& State() const { return VehicleState; }
	double Dt() const { return StepDt; }

private:
	FGTOVehicleParams Params;
	FGTOFlatGround Ground;
	FGTOChassisKinematics Body;
	FGTOVehicleState VehicleState;
	double StepDt;
};
