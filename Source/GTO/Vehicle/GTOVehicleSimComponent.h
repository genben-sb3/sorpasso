#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HAL/CriticalSection.h"
#include "PhysicsEngine/BodyInstance.h"
#include "ultragravity/input_smoothing.h"
#include "ultragravity/vehicle_file.h"
#include "ultragravity/vehicle_step.h"
#include "Vehicle/GTOInputQueue.h"
#include "Vehicle/GTOStateInterpolation.h"
#include "Vehicle/GTOVehicleTypes.h"
#include "GTOVehicleSimComponent.generated.h"

class UPrimitiveComponent;

/**
 * The adapter between Unreal and the physics package (ADR 0001). In the async physics tick of every Chaos step it sweeps
 * one wheel-sized sphere along each probe ray of the package, runs the vehicle step against those wheel contacts and
 * applies the forces, averaged over the package's substeps, to the chassis body. Driver input arrives through a queue
 * and becomes the input packet of the tick, a human's through the package's input smoothing. The game thread reads the
 * results only as copied snapshots.
 *
 * The only place that converts between Unreal (cm, Y right, left-handed) and the package (SI, ISO 8855: Y left).
 * Everything it hands out is in metres with Unreal axes.
 */
UCLASS(ClassGroup = (GTO), meta = (BlueprintSpawnableComponent))
class GTO_API UGTOVehicleSimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTOVehicleSimComponent();

	/** Trace channel of the wheel sweeps. */
	UPROPERTY(EditAnywhere, Category = "Simulation")
	TEnumAsByte<ECollisionChannel> WheelTraceChannel = ECC_Visibility;

	/** Starts simulating Body with InParams from its current pose, at rest with the engine idling. Game thread. */
	void Restart(UPrimitiveComponent* Body, const ultragravity::VehicleParams& InParams);

	/** Queues driver input for the next sim tick. Game thread. */
	void QueueInput(const FGTODriverInput& Input);

	/** False until the first Restart. */
	bool HasParams() const;

	/** kg */
	double GetMass() const;

	/** Height of the centre of mass above flat ground with the car level and every wheel hanging at full droop (m). */
	static double FullDroopHeight(const ultragravity::VehicleParams& Params);

	/** Centre of a wheel compressed by Compression (m), relative to the centre of mass in chassis axes (m). */
	FVector GetWheelCentre(EGTOWheel Wheel, double Compression) const;

	/** m */
	double GetWheelRadius(EGTOWheel Wheel) const;

	/** Origin of the vehicle file's frame relative to the centre of mass, in chassis axes (m). */
	FVector GetVehicleFrameOrigin() const;

	/**
	 * Snapshot after the last completed Chaos step: the chassis as Chaos left it, the wheels as the physics package left
	 * them. OutSimTime receives the sim time of the snapshot (s): fixed steps since the component started, across restarts.
	 */
	FGTOVehicleState GetLatestState(double* OutSimTime = nullptr) const;

	/** Snapshot interpolated between the last two for the current frame. */
	FGTOVehicleState GetDisplayState() const;

	/** Sim ticks since the component started. */
	uint32 GetSimulatedTicks() const;

	/** Sim ticks per wall-clock second, measured over the last second. */
	double GetTicksPerSecond() const { return TicksPerSecond; }

	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	ultragravity::WheelContacts SweepWheels(const ultragravity::BodyMotion& Body) const;
	ultragravity::InputPacket MakePacket(const FGTODriverInput& Input, const ultragravity::BodyMotion& Body, double Dt);
	void ApplyMassProperties(FBodyInstanceAsyncPhysicsTickHandle& Handle) const;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> ChassisBody;

	FGTOInputQueue InputQueue;

	// Shared between the async physics tick and the game thread.
	mutable FCriticalSection Lock;
	bool bHasParams = false;
	ultragravity::VehicleParams Params;
	ultragravity::State State; // carried from step to step
	ultragravity::SmoothingParams SmoothingParams;
	ultragravity::SmoothingState Smoothing; // carried from step to step
	FGTOVehicleState Stepped; // wheels, gear and engine as the last step left them; its chassis is unused
	FGTOVehicleState Latest; // see GetLatestState
	double LatestSimTime = 0.0;
	double NextSimTime = 0.0;
	FGTOSnapshotInterpolator Display;
	uint32 SimulatedTicks = 0;

	// Game thread only.
	double TicksPerSecond = 0.0;
	double RateWindowStart = 0.0;
	uint32 RateWindowTicks = 0;
};
