#pragma once

#include "GTORigidBody.h"
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

/**
 * Runs the sim core over many ticks without an Unreal world: flat ground in, chassis integrated here.
 * With NumSubsteps > 1 each tick works like the Unreal adapter on a Chaos step: contacts once per tick, the sim core
 * substepped against them, and the chassis moved once per tick by the averaged forces.
 */
class GTOSIM_API FGTOHeadlessHarness
{
public:
	static constexpr double Gravity = 9.81; // m/s²

	FGTOHeadlessHarness(const FGTOVehicleParams& InParams, const FGTOFlatGround& InGround, const FGTOChassisKinematics& Start, double InDt = 1.0 / 960.0, int32 InNumSubsteps = 1);

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
	int32 NumSubsteps;
};
