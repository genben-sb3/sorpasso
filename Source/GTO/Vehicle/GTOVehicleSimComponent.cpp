#include "Vehicle/GTOVehicleSimComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GTOVehicleSim.h"
#include "Misc/ScopeLock.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Vehicle/GTOUnits.h"

using namespace GTOUnits;

UGTOVehicleSimComponent::UGTOVehicleSimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after the frame's Chaos steps, see FGTOSnapshotInterpolator
	bAutoActivate = true; // Chaos asserts on inactive components with an async tick (ADR 0001)
	SetAsyncPhysicsTickEnabled(true);
}

void UGTOVehicleSimComponent::Restart(UPrimitiveComponent* Body, const FGTOVehicleParams& InParams)
{
	check(Body);
	FScopeLock ScopeLock(&Lock);
	ChassisBody = Body;
	Params = InParams;
	State = FGTOVehicleState();
	State.Chassis.Position = Body->GetComponentLocation() / CmPerM;
	State.Chassis.Orientation = Body->GetComponentQuat();
	Latest = State;
	LatestSimTime = NextSimTime;
	Display.Reset(State, UPhysicsSettings::Get()->AsyncFixedTimeStepSize);
	InputQueue.Reset();
}

void UGTOVehicleSimComponent::QueueInput(FGTOInputPacket Packet)
{
	{
		FScopeLock ScopeLock(&Lock);
		Packet.Tick = SimulatedTicks; // the next tick to simulate
	}
	InputQueue.Push(Packet);
}

FGTOVehicleParams UGTOVehicleSimComponent::GetParams() const
{
	FScopeLock ScopeLock(&Lock);
	return Params;
}

FGTOVehicleState UGTOVehicleSimComponent::GetLatestState(double* OutSimTime) const
{
	FScopeLock ScopeLock(&Lock);
	if (OutSimTime)
	{
		*OutSimTime = LatestSimTime;
	}
	return Latest;
}

FGTOVehicleState UGTOVehicleSimComponent::GetDisplayState() const
{
	FScopeLock ScopeLock(&Lock);
	return Display.Sample();
}

uint32 UGTOVehicleSimComponent::GetSimulatedTicks() const
{
	FScopeLock ScopeLock(&Lock);
	return SimulatedTicks;
}

void UGTOVehicleSimComponent::AsyncPhysicsTickComponent(float DeltaTime, float SimTime)
{
	Super::AsyncPhysicsTickComponent(DeltaTime, SimTime);
	if (!ChassisBody)
	{
		return; // not started yet
	}
	FBodyInstanceAsyncPhysicsTickHandle Handle = ChassisBody->GetBodyInstanceAsyncPhysicsTickHandle();
	if (!Handle)
	{
		return;
	}

	FScopeLock ScopeLock(&Lock);
	ApplyMassProperties(Handle);
	const FGTOChassisKinematics Chassis = ReadChassis(Handle);

	// Snapshot of the previous step: its wheels, with the chassis where Chaos actually moved it (collisions included)
	// rather than where the substeps predicted it.
	Latest = State;
	Latest.Chassis = Chassis;
	LatestSimTime = NextSimTime;
	Display.Push(Latest);

	const FGTOWheelContacts Contacts = CastWheels(Chassis);
	const FGTOInputPacket Packet = InputQueue.Pop(SimulatedTicks);
	const FVector Gravity(0.0, 0.0, GetWorld()->GetGravityZ() / CmPerM);

	const FGTOStepResult Result = FGTOVehicleSim::StepSubstepped(State, Params, Packet, Contacts, Chassis, Gravity, DeltaTime, NumSubsteps);
	State = Result.State;
	++SimulatedTicks;
	NextSimTime += DeltaTime;

	// Forces through the handle directly: UPhysicsThreadLibrary::AddForce is not callable in 5.8 (ADR 0001).
	Handle->AddForce(Result.Forces.Force * CmPerM);
	Handle->AddTorque(Result.Forces.Torque * Cm2PerM2);
}

void UGTOVehicleSimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FScopeLock ScopeLock(&Lock);
	Display.Advance(DeltaTime);

	const double Now = FPlatformTime::Seconds();
	if (RateWindowStart == 0.0)
	{
		RateWindowStart = Now;
		RateWindowTicks = SimulatedTicks;
	}
	else if (Now - RateWindowStart >= 1.0)
	{
		TicksPerSecond = (SimulatedTicks - RateWindowTicks) / (Now - RateWindowStart);
		RateWindowStart = Now;
		RateWindowTicks = SimulatedTicks;
	}
}

FGTOChassisKinematics UGTOVehicleSimComponent::ReadChassis(FBodyInstanceAsyncPhysicsTickHandle& Handle) const
{
	const FQuat Rotation(Handle->R());
	FGTOChassisKinematics Chassis;
	Chassis.Position = (FVector(Handle->X()) + Rotation.RotateVector(FVector(Handle->CenterOfMass()))) / CmPerM;
	Chassis.Orientation = Rotation;
	Chassis.LinearVelocity = FVector(Handle->V()) / CmPerM;
	Chassis.AngularVelocity = FVector(Handle->W());
	return Chassis;
}

FGTOWheelContacts UGTOVehicleSimComponent::CastWheels(const FGTOChassisKinematics& Chassis) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GTOWheelCast), false, GetOwner());
	Query.bReturnPhysicalMaterial = true;
	const FVector Down = Chassis.Orientation.RotateVector(-FVector::UpVector);

	FGTOWheelContacts Contacts;
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const EGTOWheel Wheel = static_cast<EGTOWheel>(Index);
		const FGTOAxleParams& Axle = Params.Axle(Wheel);
		const double Radius = Axle.WheelRadius * CmPerM;
		const FVector Mount = Params.WheelMountWorld(Wheel, Chassis) * CmPerM;

		// Wheel-sized sphere from one radius above the mount to full droop, so the ground is found even at full compression.
		const FVector Start = Mount - Down * Radius;
		FHitResult Hit;
		if (!GetWorld()->SweepSingleByChannel(Hit, Start, Mount + Down * (Axle.Suspension.MaxLength * CmPerM),
			FQuat::Identity, WheelTraceChannel, FCollisionShape::MakeSphere(Radius), Query))
		{
			continue;
		}

		FGTOWheelContact& Contact = Contacts[Index];
		Contact.bHit = true;
		// The surface's grip is its physical material's friction: 1 for dry asphalt (PM_Asphalt). Surfaces without a
		// material of their own report Unreal's default material, friction 0.7; no material at all counts as asphalt.
		Contact.Friction = Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->Friction : 1.0;
		if (Hit.bStartPenetrating)
		{
			// Ground already above the start, e.g. a hard landing on a kerb: no impact point, but the sphere would be pushed
			// out along Normal by PenetrationDepth, so the ground plane touches the sphere's surface there. The wheel stays
			// in contact far beyond the bump stop and pushes the body up instead of dropping through.
			Contact.Normal = Hit.Normal;
			Contact.Point = (Start - Hit.Normal * (Radius - Hit.PenetrationDepth)) / CmPerM;
		}
		else
		{
			Contact.Point = Hit.ImpactPoint / CmPerM;
			Contact.Normal = Hit.ImpactNormal;
		}
	}
	return Contacts;
}

void UGTOVehicleSimComponent::ApplyMassProperties(FBodyInstanceAsyncPhysicsTickHandle& Handle) const
{
	// Chaos derives mass and inertia from the collision shape; the sim core's values win. Checked every step, because
	// the game thread may push shape-derived values again, e.g. after a mass override.
	if (!FMath::IsNearlyEqual(Handle->M(), Params.Mass, 1.0e-3 * Params.Mass))
	{
		Handle->SetM(Params.Mass);
		Handle->SetInvM(1.0 / Params.Mass);
	}

	const FVector Inertia = Params.Inertia * Cm2PerM2;
	const Chaos::TVec3<Chaos::FRealSingle> DesiredI(Inertia.X, Inertia.Y, Inertia.Z);
	if (!Handle->I().Equals(DesiredI, 1.0e-3f * DesiredI.GetMax()))
	{
		Handle->SetI(DesiredI);
		Handle->SetInvI(Chaos::TVec3<Chaos::FRealSingle>(1.f / DesiredI.X, 1.f / DesiredI.Y, 1.f / DesiredI.Z));
	}
}
