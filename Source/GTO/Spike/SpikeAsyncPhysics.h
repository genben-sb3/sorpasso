// PROTOTYPE – Wegwerf-Spike für Issue #2 (Async-Physik auf UE 5.8). Kein Produktionscode.
// Lebt nur auf dem Branch prototype/async-physics-spike.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/CriticalSection.h"
#include "SpikeAsyncPhysics.generated.h"

class UStaticMeshComponent;

/** Misst im Async-Physics-Tick: Thread, dt, Sim-Zeit, Sweep-Ergebnis, Kraftwirkung. */
UCLASS()
class USpikeAsyncProbe : public UActorComponent
{
	GENERATED_BODY()

public:
	USpikeAsyncProbe();

	virtual void BeginPlay() override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Body;

private:
	struct FWindow
	{
		int64 Steps = 0;
		int64 StepsOnGameThread = 0;
		int64 SweepCalls = 0;
		int64 SweepHits = 0;
		double SimTimeFirst = -1.0;
		double SimTimeLast = -1.0;
		float MinDt = TNumericLimits<float>::Max();
		float MaxDt = 0.f;
		uint32 LastThreadId = 0;
		double LastHitZ = 0.0;
		double LastVelX = 0.0;
	};

	FCriticalSection Lock;
	FWindow Window;
	FWindow Total;

	double ForceStartSimTime = -1.0;
	double WallStart = 0.0;
	double WindowWallStart = 0.0;
	int64 WindowFrames = 0;
	int64 TotalFrames = 0;
	bool bReported = false;
	bool bWakeEveryTick = false;
};

UCLASS()
class ASpikeBody : public AActor
{
	GENERATED_BODY()

public:
	ASpikeBody();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpikeAsyncProbe> Probe;
};

UCLASS()
class ASpikeFloor : public AActor
{
	GENERATED_BODY()

public:
	ASpikeFloor();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};

/** Spawnt Boden und Körper. Start: <Map>?game=/Script/GTO.SpikeGameMode */
UCLASS()
class ASpikeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
