#include "Vehicle/GTOVehicleSimComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeLock.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Vehicle/GTOUnits.h"

using namespace GTOUnits;

namespace
{
	// Unreal's axes are those of ISO 8855 mirrored at the XZ plane (Y right instead of left). A position or velocity
	// flips its Y; an axial vector (angular velocity, torque) is mirrored as well and changes sign, so it flips X and Z;
	// a rotation keeps its angle about the mirrored axis but turns the other way.

	ultragravity::Vec3 ToPackage(const FVector& V)
	{
		return { V.X, -V.Y, V.Z };
	}

	FVector ToUnreal(const ultragravity::Vec3& V)
	{
		return FVector(V.x, -V.y, V.z);
	}

	ultragravity::Vec3 AxialToPackage(const FVector& V)
	{
		return { -V.X, V.Y, -V.Z };
	}

	FVector AxialToUnreal(const ultragravity::Vec3& V)
	{
		return FVector(-V.x, V.y, -V.z);
	}

	ultragravity::Quat ToPackage(const FQuat& Q)
	{
		return { Q.W, -Q.X, Q.Y, -Q.Z };
	}

	/** Physics package body motion of the Chaos body, its position at the centre of mass. */
	ultragravity::BodyMotion ReadBody(FBodyInstanceAsyncPhysicsTickHandle& Handle)
	{
		const FQuat Rotation(Handle->R());
		const FVector CentreOfMass = FVector(Handle->X()) + Rotation.RotateVector(FVector(Handle->CenterOfMass())); // cm
		ultragravity::BodyMotion Body;
		Body.position = ToPackage(CentreOfMass / CmPerM);
		Body.orientation = ToPackage(Rotation);
		Body.velocity = ToPackage(FVector(Handle->V()) / CmPerM);
		Body.angular_velocity = AxialToPackage(FVector(Handle->W()));
		return Body;
	}

	FGTOChassisKinematics ToChassis(const ultragravity::BodyMotion& Body)
	{
		FGTOChassisKinematics Chassis;
		Chassis.Position = ToUnreal(Body.position);
		Chassis.Orientation = FQuat(-Body.orientation.x, Body.orientation.y, -Body.orientation.z, Body.orientation.w);
		Chassis.LinearVelocity = ToUnreal(Body.velocity);
		Chassis.AngularVelocity = AxialToUnreal(Body.angular_velocity);
		return Chassis;
	}

	ultragravity::InputPacket ToPacket(const FGTODriverInput& Input, uint32 Tick)
	{
		ultragravity::InputPacket Packet;
		Packet.tick = Tick;
		Packet.steering = -FMath::Clamp(Input.Steer, -1.f, 1.f); // the package steers positive to the left
		Packet.throttle = FMath::Clamp(Input.Throttle, 0.f, 1.f);
		Packet.brake = FMath::Clamp(Input.Brake, 0.f, 1.f);
		Packet.handbrake = FMath::Clamp(Input.Handbrake, 0.f, 1.f);
		Packet.gear_request = Input.GearRequest;
		return Packet;
	}

	/** The car's wheels as one step of the physics package left them. */
	TStaticArray<FGTOWheelState, GTONumWheels> ToWheels(const ultragravity::StepResult& Result, const ultragravity::WheelContacts& Contacts)
	{
		TStaticArray<FGTOWheelState, GTONumWheels> Wheels;
		for (int32 Index = 0; Index < GTONumWheels; ++Index)
		{
			FGTOWheelState& Wheel = Wheels[Index];
			Wheel.Compression = Result.animation.suspension_travel[Index];
			Wheel.Load = Result.telemetry.wheel_load[Index];
			Wheel.AngularVelocity = Result.state.wheel_speed[Index];
			Wheel.SteerAngle = -Result.animation.steer_angle[Index];
			Wheel.SlipRatio = Result.telemetry.slip[Index];
			Wheel.SlipAngle = -Result.telemetry.slip_angle[Index];
			Wheel.LongitudinalForce = Result.telemetry.force_longitudinal[Index];
			Wheel.LateralForce = -Result.telemetry.force_lateral[Index];
			Wheel.SurfaceFriction = Contacts[Index].contact ? Contacts[Index].friction : 0.0;
		}
		return Wheels;
	}

	/** Probe rays of the car standing level with its centre of mass at the origin: the wheel mounts in chassis axes. */
	std::array<ultragravity::ProbeRay, ultragravity::wheel_count> RestingProbeRays(const ultragravity::VehicleParams& Params)
	{
		return ultragravity::probe_rays(Params, ultragravity::BodyMotion());
	}
}

UGTOVehicleSimComponent::UGTOVehicleSimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after the frame's Chaos steps, see FGTOSnapshotInterpolator
	bAutoActivate = true; // Chaos asserts on inactive components with an async tick (ADR 0001)
	SetAsyncPhysicsTickEnabled(true);
}

void UGTOVehicleSimComponent::Restart(UPrimitiveComponent* Body, const ultragravity::VehicleParams& InParams)
{
	check(Body);
	FScopeLock ScopeLock(&Lock);
	ChassisBody = Body;
	Params = InParams;
	bHasParams = true;
	State = ultragravity::rolling_state(Params, 0.0);
	Wheels = TStaticArray<FGTOWheelState, GTONumWheels>();
	Latest = FGTOVehicleState();
	Latest.Chassis.Position = Body->GetComponentLocation() / CmPerM;
	Latest.Chassis.Orientation = Body->GetComponentQuat();
	LatestSimTime = NextSimTime;
	Display.Reset(Latest, UPhysicsSettings::Get()->AsyncFixedTimeStepSize);
	InputQueue.Reset();
}

void UGTOVehicleSimComponent::QueueInput(const FGTODriverInput& Input)
{
	uint32 NextTick = 0;
	{
		FScopeLock ScopeLock(&Lock);
		NextTick = SimulatedTicks;
	}
	InputQueue.Push(NextTick, Input);
}

bool UGTOVehicleSimComponent::HasParams() const
{
	FScopeLock ScopeLock(&Lock);
	return bHasParams;
}

double UGTOVehicleSimComponent::GetMass() const
{
	FScopeLock ScopeLock(&Lock);
	return Params.mass;
}

double UGTOVehicleSimComponent::FullDroopHeight(const ultragravity::VehicleParams& Params)
{
	double Lowest = 0.0; // m
	for (const ultragravity::ProbeRay& Ray : RestingProbeRays(Params))
	{
		Lowest = FMath::Min(Lowest, (Ray.start + Ray.direction * Ray.length).z);
	}
	return -Lowest;
}

FVector UGTOVehicleSimComponent::GetWheelCentre(EGTOWheel Wheel, double Compression) const
{
	FScopeLock ScopeLock(&Lock);
	// The probe ray runs from the mount through the wheel centre to the bottom of the tyre at full droop.
	const ultragravity::ProbeRay Ray = RestingProbeRays(Params)[static_cast<int32>(Wheel)];
	const ultragravity::Axle& Axle = GTOIsFront(Wheel) ? Params.front : Params.rear;
	return ToUnreal(Ray.start + Ray.direction * (Ray.length - Axle.wheel_radius - Compression));
}

double UGTOVehicleSimComponent::GetWheelRadius(EGTOWheel Wheel) const
{
	FScopeLock ScopeLock(&Lock);
	return (GTOIsFront(Wheel) ? Params.front : Params.rear).wheel_radius;
}

FVector UGTOVehicleSimComponent::GetVehicleFrameOrigin() const
{
	FScopeLock ScopeLock(&Lock);
	return ToUnreal(-Params.center_of_mass);
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
	const ultragravity::BodyMotion Body = ReadBody(Handle);

	// Snapshot of the previous step: its wheels, with the chassis where Chaos actually moved it (collisions included).
	Latest.Chassis = ToChassis(Body);
	Latest.Wheels = Wheels;
	LatestSimTime = NextSimTime;
	Display.Push(Latest);

	const ultragravity::WheelContacts Contacts = SweepWheels(Body);
	const ultragravity::InputPacket Packet = ToPacket(InputQueue.Pop(SimulatedTicks), SimulatedTicks);
	const ultragravity::StepResult Result = ultragravity::vehicle_step(State, Params, Packet, Contacts, Body, DeltaTime);
	State = Result.state;
	Wheels = ToWheels(Result, Contacts);
	++SimulatedTicks;
	NextSimTime += DeltaTime;

	// Forces through the handle directly: UPhysicsThreadLibrary::AddForce is not callable in 5.8 (ADR 0001).
	Handle->AddForce(ToUnreal(Result.forces.force) * CmPerM);
	Handle->AddTorque(AxialToUnreal(Result.forces.torque) * Cm2PerM2);
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

ultragravity::WheelContacts UGTOVehicleSimComponent::SweepWheels(const ultragravity::BodyMotion& Body) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GTOWheelSweep), false, GetOwner());
	Query.bReturnPhysicalMaterial = true;
	const std::array<ultragravity::ProbeRay, ultragravity::wheel_count> Rays = ultragravity::probe_rays(Params, Body);

	ultragravity::WheelContacts Contacts;
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const ultragravity::ProbeRay& Ray = Rays[Index];
		const ultragravity::Axle& Axle = GTOIsFront(static_cast<EGTOWheel>(Index)) ? Params.front : Params.rear;
		const double Radius = Axle.wheel_radius * CmPerM;
		const FVector Mount = ToUnreal(Ray.start) * CmPerM;
		const FVector Down = ToUnreal(Ray.direction);

		// A wheel-sized sphere from one radius above the mount to the wheel centre at full droop, so the ground is found
		// even at full compression. Its lowest point covers the probe ray.
		const FVector Start = Mount - Down * Radius;
		const FVector End = Mount + Down * ((Ray.length - Axle.wheel_radius) * CmPerM);
		FHitResult Hit;
		if (!GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, WheelTraceChannel, FCollisionShape::MakeSphere(Radius), Query))
		{
			continue;
		}

		ultragravity::WheelContact& Contact = Contacts[Index];
		Contact.contact = true;
		// The surface's grip is its physical material's friction: 1 for dry asphalt (PM_Asphalt). Surfaces without a
		// material of their own report Unreal's default material, friction 0.7; no material at all counts as asphalt.
		Contact.friction = Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->Friction : 1.0;
		if (Hit.bStartPenetrating)
		{
			// Ground already above the start, e.g. a hard landing on a curb: no impact point, but the sphere would be pushed
			// out along Normal by PenetrationDepth, so the ground plane touches the sphere's surface there. The wheel stays
			// in contact far beyond the bump stop and pushes the body up instead of dropping through.
			Contact.normal = ToPackage(Hit.Normal);
			Contact.point = ToPackage((Start - Hit.Normal * (Radius - Hit.PenetrationDepth)) / CmPerM);
		}
		else
		{
			Contact.normal = ToPackage(Hit.ImpactNormal);
			Contact.point = ToPackage(Hit.ImpactPoint / CmPerM);
		}
	}
	return Contacts;
}

void UGTOVehicleSimComponent::ApplyMassProperties(FBodyInstanceAsyncPhysicsTickHandle& Handle) const
{
	// Chaos derives mass and inertia from the collision shape; the vehicle file's values win. Checked every step, because
	// the game thread may push shape-derived values again, e.g. after a mass override. The principal moments are the same
	// in both axis conventions.
	if (!FMath::IsNearlyEqual(Handle->M(), Params.mass, 1.0e-3 * Params.mass))
	{
		Handle->SetM(Params.mass);
		Handle->SetInvM(1.0 / Params.mass);
	}

	const FVector Inertia = FVector(Params.inertia.x, Params.inertia.y, Params.inertia.z) * Cm2PerM2;
	const Chaos::TVec3<Chaos::FRealSingle> DesiredI(Inertia.X, Inertia.Y, Inertia.Z);
	if (!Handle->I().Equals(DesiredI, 1.0e-3f * DesiredI.GetMax()))
	{
		Handle->SetI(DesiredI);
		Handle->SetInvI(Chaos::TVec3<Chaos::FRealSingle>(1.f / DesiredI.X, 1.f / DesiredI.Y, 1.f / DesiredI.Z));
	}
}
