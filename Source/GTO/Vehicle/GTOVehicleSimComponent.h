#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTOInputQueue.h"
#include "GTOStateInterpolation.h"
#include "GTOVehicleParams.h"
#include "GTOVehicleState.h"
#include "HAL/CriticalSection.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GTOVehicleSimComponent.generated.h"

class UPrimitiveComponent;

/**
 * Unreal adapter of the sim core (ADR 0001). In the async physics tick of every Chaos step it casts one sphere per
 * wheel, runs the sim core in substeps against those contacts and applies the averaged forces to the chassis body.
 * Input arrives as packets through a queue; the game thread reads the results only as copied snapshots.
 */
UCLASS(ClassGroup = (GTO), meta = (BlueprintSpawnableComponent))
class GTO_API UGTOVehicleSimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTOVehicleSimComponent();

	/** Sim core substeps per Chaos step. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = 1))
	int32 NumSubsteps = 8;

	/** Trace channel of the wheel casts. */
	UPROPERTY(EditAnywhere, Category = "Simulation")
	TEnumAsByte<ECollisionChannel> WheelTraceChannel = ECC_Visibility;

	/** Starts simulating Body with InParams from its current pose, wheels unloaded. Game thread. */
	void Restart(UPrimitiveComponent* Body, const FGTOVehicleParams& InParams);

	/** Queues controls for the next sim tick. Game thread. */
	void QueueInput(FGTOInputPacket Packet);

	FGTOVehicleParams GetParams() const;

	/** Snapshot after the last completed Chaos step: the chassis as Chaos left it, the wheels as the sim core left them. */
	FGTOVehicleState GetLatestState() const;

	/** Snapshot interpolated between the last two for the current frame. */
	FGTOVehicleState GetDisplayState() const;

	/** Sim ticks since the component started. */
	uint32 GetSimulatedTicks() const;

	/** Sim ticks per wall-clock second, measured over the last second. */
	double GetTicksPerSecond() const { return TicksPerSecond; }

	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FGTOChassisKinematics ReadChassis(FBodyInstanceAsyncPhysicsTickHandle& Handle) const;
	FGTOWheelContacts CastWheels(const FGTOChassisKinematics& Chassis) const;
	void ApplyMassProperties(FBodyInstanceAsyncPhysicsTickHandle& Handle) const;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> ChassisBody;

	FGTOInputQueue InputQueue;

	// Shared between the async physics tick and the game thread.
	mutable FCriticalSection Lock;
	FGTOVehicleParams Params;
	FGTOVehicleState State;  // carried from step to step
	FGTOVehicleState Latest; // see GetLatestState
	FGTOSnapshotInterpolator Display;
	uint32 SimulatedTicks = 0;

	// Game thread only.
	double TicksPerSecond = 0.0;
	double RateWindowStart = 0.0;
	uint32 RateWindowTicks = 0;
};
