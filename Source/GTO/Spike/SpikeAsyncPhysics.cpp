// PROTOTYPE – Wegwerf-Spike für Issue #2 (Async-Physik auf UE 5.8). Kein Produktionscode.

#include "SpikeAsyncPhysics.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "Misc/ScopeLock.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/Experimental/PhysicsThreadLibrary.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PBDRigidsSolver.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<float> CVarSpikeGameThreadSleepMs(
	TEXT("spike.GameThreadSleepMs"), 0.f, TEXT("Künstliche Last pro Game-Thread-Frame in ms (drosselt die Bildrate)."));
static TAutoConsoleVariable<float> CVarSpikeDurationSec(
	TEXT("spike.DurationSec"), 12.f, TEXT("Laufzeit in Wanduhr-Sekunden, danach Bericht und Beenden."));

static TAutoConsoleVariable<float> CVarSpikeAccel(
	TEXT("spike.Accel"), 100.f, TEXT("Konstante Beschleunigung in X in cm/s^2."));
static TAutoConsoleVariable<bool> CVarSpikeDisableSleep(
	TEXT("spike.DisableSleep"), false, TEXT("Schlafen des Körpers verhindern (Custom Sleep Family, Multiplikator 0)."));
static TAutoConsoleVariable<bool> CVarSpikeWake(
	TEXT("spike.Wake"), false, TEXT("Körper in jedem Async-Tick explizit wecken."));

// Konstante Beschleunigung in X, damit die Kraftwirkung prüfbar ist: v = a * t. Wird in BeginPlay aus spike.Accel gelesen.
static double SpikeAccelCmPerS2 = 100.0;

USpikeAsyncProbe::USpikeAsyncProbe()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Ohne Aktivierung stürzt Chaos im Async-Tick ab: check(Component->IsActive()) in PhysScene_Chaos.cpp.
	bAutoActivate = true;
	SetAsyncPhysicsTickEnabled(true);
}

void USpikeAsyncProbe::BeginPlay()
{
	Super::BeginPlay();
	WallStart = WindowWallStart = FPlatformTime::Seconds();
	SpikeAccelCmPerS2 = CVarSpikeAccel.GetValueOnGameThread();
	bWakeEveryTick = CVarSpikeWake.GetValueOnGameThread();
	if (Body && CVarSpikeDisableSleep.GetValueOnGameThread())
	{
		Body->BodyInstance.SleepFamily = ESleepFamily::Custom;
		Body->BodyInstance.CustomSleepThresholdMultiplier = 0.f;
		Body->RecreatePhysicsState();
	}

	const UPhysicsSettings* Settings = UPhysicsSettings::Get();
	double SolverAsyncDt = -1.0;
	if (FPhysScene_Chaos* Scene = GetWorld()->GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = Scene->GetSolver())
		{
			SolverAsyncDt = Solver->GetAsyncDeltaTime();
		}
	}
	UE_LOG(LogTemp, Display, TEXT("SPIKE settings: TickPhysicsAsync=%d AsyncFixedTimeStepSize=%.6f SolverAsyncDt=%.6f MaxPhysicsDeltaTime=%.6f SubsteppingAsync=%d GameThreadId=%u SleepMs=%.1f Accel=%.1f DisableSleep=%d Wake=%d"),
		Settings->bTickPhysicsAsync, Settings->AsyncFixedTimeStepSize, SolverAsyncDt, Settings->MaxPhysicsDeltaTime,
		Settings->bSubsteppingAsync, GGameThreadId, CVarSpikeGameThreadSleepMs.GetValueOnGameThread(),
		SpikeAccelCmPerS2, CVarSpikeDisableSleep.GetValueOnGameThread(), bWakeEveryTick);
}

void USpikeAsyncProbe::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);
	if (!Body)
	{
		return;
	}

	const bool bOnGameThread = IsInGameThread();
	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	FBodyInstanceAsyncPhysicsTickHandle Handle = Body->GetBodyInstanceAsyncPhysicsTickHandle();
	double VelX = 0.0;
	FVector Pos = FVector::ZeroVector;
	if (Handle)
	{
		Pos = FVector(Handle->X());
		VelX = Handle->V().X;
		if (bWakeEveryTick)
		{
			Handle->SetObjectState(Chaos::EObjectStateType::Dynamic);
		}
		// UPhysicsThreadLibrary::AddForce ist in 5.8 private (fehlendes public:), daher direkt wie dort intern.
		Handle->AddForce(Chaos::FVec3(SpikeAccelCmPerS2, 0.0, 0.0) * Handle->M());
	}

	// Frage 1b: Ist ein World-Sweep hier erlaubt?
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpikeSweep), false, GetOwner());
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Pos, Pos - FVector(0, 0, 1000), FQuat::Identity,
		ECC_WorldStatic, FCollisionShape::MakeSphere(20.f), Params);

	FScopeLock ScopeLock(&Lock);
	if (ForceStartSimTime < 0.0)
	{
		ForceStartSimTime = SimTime;
	}
	for (FWindow* W : { &Window, &Total })
	{
		++W->Steps;
		W->StepsOnGameThread += bOnGameThread ? 1 : 0;
		++W->SweepCalls;
		W->SweepHits += bHit ? 1 : 0;
		if (W->SimTimeFirst < 0.0)
		{
			W->SimTimeFirst = SimTime;
		}
		W->SimTimeLast = SimTime;
		W->MinDt = FMath::Min(W->MinDt, DeltaTime);
		W->MaxDt = FMath::Max(W->MaxDt, DeltaTime);
		W->LastThreadId = ThreadId;
		W->LastHitZ = bHit ? Hit.Location.Z : 0.0;
		W->LastVelX = VelX;
	}
}

void USpikeAsyncProbe::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float SleepMs = CVarSpikeGameThreadSleepMs.GetValueOnGameThread();
	if (SleepMs > 0.f)
	{
		FPlatformProcess::Sleep(SleepMs / 1000.f);
	}

	++WindowFrames;
	++TotalFrames;
	const double Now = FPlatformTime::Seconds();
	const double WindowWall = Now - WindowWallStart;
	if (WindowWall < 1.0)
	{
		return;
	}

	FWindow W;
	double ForceStart;
	{
		FScopeLock ScopeLock(&Lock);
		W = Window;
		Window = FWindow();
		ForceStart = ForceStartSimTime;
	}

	const double SimAdvanced = W.SimTimeLast - W.SimTimeFirst;
	const double ExpectedVelX = SpikeAccelCmPerS2 * (W.SimTimeLast - ForceStart);
	UE_LOG(LogTemp, Display, TEXT("SPIKE window: wall=%.3fs gtFps=%.1f steps=%lld steps/s=%.1f simAdvanced=%.3fs sim/wall=%.3f dt=[%.6f..%.6f] onGT=%lld/%lld thread=%u sweepHits=%lld/%lld hitZ=%.1f velX=%.1f expected=%.1f"),
		WindowWall, WindowFrames / WindowWall, W.Steps, W.Steps / WindowWall, SimAdvanced, SimAdvanced / WindowWall,
		W.MinDt, W.MaxDt, W.StepsOnGameThread, W.Steps, W.LastThreadId, W.SweepHits, W.SweepCalls, W.LastHitZ,
		W.LastVelX, ExpectedVelX);
	WindowWallStart = Now;
	WindowFrames = 0;

	const double TotalWall = Now - WallStart;
	if (!bReported && TotalWall >= CVarSpikeDurationSec.GetValueOnGameThread())
	{
		bReported = true;
		FWindow T;
		{
			FScopeLock ScopeLock(&Lock);
			T = Total;
		}
		const double TotalSim = T.SimTimeLast - T.SimTimeFirst;
		UE_LOG(LogTemp, Display, TEXT("SPIKE total: wall=%.3fs gtFps=%.1f steps=%lld steps/s=%.1f sim/wall=%.3f dt=[%.6f..%.6f] onGT=%lld/%lld sweepHits=%lld/%lld velX=%.1f expected=%.1f"),
			TotalWall, TotalFrames / TotalWall, T.Steps, T.Steps / TotalWall, TotalSim / TotalWall, T.MinDt, T.MaxDt,
			T.StepsOnGameThread, T.Steps, T.SweepHits, T.SweepCalls, T.LastVelX,
			SpikeAccelCmPerS2 * (T.SimTimeLast - ForceStart));
		FPlatformMisc::RequestExit(false, TEXT("Spike finished"));
	}
}

ASpikeBody::ASpikeBody()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(false);
	Mesh->SetLinearDamping(0.f);
	Mesh->SetAngularDamping(0.f);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	RootComponent = Mesh;

	Probe = CreateDefaultSubobject<USpikeAsyncProbe>(TEXT("Probe"));
	Probe->Body = Mesh;
}

ASpikeFloor::ASpikeFloor()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Mesh;
}

void ASpikeGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Boden: 2 km x 50 m, Oberkante bei z = 50 cm.
	ASpikeFloor* Floor = GetWorld()->SpawnActor<ASpikeFloor>(FVector::ZeroVector, FRotator::ZeroRotator);
	Floor->SetActorScale3D(FVector(2000.0, 50.0, 1.0));

	// Körper schwebt ohne Schwerkraft bei z = 300 cm; ein Kugel-Sweep (r = 20) trifft den Boden bei z = 70.
	ASpikeBody* Body = GetWorld()->SpawnActor<ASpikeBody>(FVector(0.0, 0.0, 300.0), FRotator::ZeroRotator);
	Body->Mesh->SetMassOverrideInKg(NAME_None, 1000.f, true);
}
