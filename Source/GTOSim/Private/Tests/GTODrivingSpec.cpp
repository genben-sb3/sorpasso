#include "Misc/AutomationTest.h"
#include "GTOSimTestCars.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GTOSimTest;

namespace
{
	FGTOInputPacket Controls(float Throttle, float Brake = 0.f)
	{
		FGTOInputPacket Packet;
		Packet.Throttle = Throttle;
		Packet.Brake = Brake;
		return Packet;
	}

	/** Car settled on its springs at rest, stepped like the adapter. */
	FGTOHeadlessHarness SettledCar(const FGTOVehicleParams& Params, const FGTOChassisKinematics& Start)
	{
		FGTOHeadlessHarness Harness = MakeAdapterLikeHarness(Params, FGTOFlatGround(), Start);
		RunFor(Harness, 2.0);
		return Harness;
	}

	/**
	 * Velocity of the chassis point on the ground below the centre of mass, in body axes. Unlike the centre of mass it
	 * does not swing with the body rocking on its springs, so it shows what the tyres do.
	 */
	FVector GroundVelocity(const FGTOChassisKinematics& Chassis)
	{
		const FVector Below(0.0, 0.0, -Chassis.Position.Z); // the harness ground lies at z = 0
		return BodyVelocity(Chassis) + Chassis.Orientation.UnrotateVector(FVector::CrossProduct(Chassis.AngularVelocity, Below));
	}

	/** Watches the chassis at ground level for jitter: speed changes against the expected direction, and blow-ups. */
	struct FJitterWatch
	{
		double LastForward = 0.0;
		double WorstReversal = 0.0;  // m/s, largest single-tick change against the expected direction
		double MaxSideSpeed = 0.0;   // m/s
		double MaxYawRate = 0.0;     // rad/s
		double MaxWheelSpeed = 0.0;  // rad/s
		bool bFinite = true;

		void Start(const FGTOHeadlessHarness& Harness)
		{
			LastForward = GroundVelocity(Harness.Chassis()).X;
		}

		/** Direction +1 while the car should only speed up, -1 while it should only slow down. */
		void Watch(const FGTOHeadlessHarness& Harness, double Direction)
		{
			const FGTOChassisKinematics& Chassis = Harness.Chassis();
			const FVector Velocity = GroundVelocity(Chassis);
			WorstReversal = FMath::Max(WorstReversal, -Direction * (Velocity.X - LastForward));
			LastForward = Velocity.X;
			MaxSideSpeed = FMath::Max(MaxSideSpeed, FMath::Abs(Velocity.Y));
			MaxYawRate = FMath::Max(MaxYawRate, FMath::Abs(Chassis.AngularVelocity.Z));
			bFinite &= !Chassis.Position.ContainsNaN() && !Chassis.LinearVelocity.ContainsNaN();
			for (const FGTOWheelState& Wheel : Harness.State().Wheels)
			{
				bFinite &= FMath::IsFinite(Wheel.AngularVelocity);
				MaxWheelSpeed = FMath::Max(MaxWheelSpeed, FMath::Abs(Wheel.AngularVelocity));
			}
		}
	};
}

BEGIN_DEFINE_SPEC(FGTODrivingSpec, "GTO.Sim.Driving", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
	void PullAwayAndStop(float Throttle, double DriveSeconds, float Brake);
END_DEFINE_SPEC(FGTODrivingSpec)

void FGTODrivingSpec::PullAwayAndStop(float Throttle, double DriveSeconds, float Brake)
{
	const FGTOVehicleParams Params = MakeSportsCar();
	FGTOHeadlessHarness Harness = SettledCar(Params, DroppedAbove(Params, FGTOFlatGround(), 0.0));
	const FVector StartPosition = Harness.Chassis().Position;

	FJitterWatch Driving;
	Driving.Start(Harness);
	RunFor(Harness, DriveSeconds, Controls(Throttle), [&]() { Driving.Watch(Harness, 1.0); });
	const double TopSpeed = BodyVelocity(Harness.Chassis()).X;
	RunFor(Harness, 1.0); // coast, so wheels spun up beyond road speed come back to it before the brake goes on

	FJitterWatch Stopping;
	Stopping.Start(Harness);
	double MinForward = TNumericLimits<double>::Max();
	RunFor(Harness, 20.0, Controls(0.f, Brake), [&]()
	{
		Stopping.Watch(Harness, -1.0);
		MinForward = FMath::Min(MinForward, GroundVelocity(Harness.Chassis()).X);
	});
	const FVector StoppedAt = Harness.Chassis().Position;
	double MaxRestSpeed = 0.0;
	RunFor(Harness, 1.0, Controls(0.f, Brake), [&]() { MaxRestSpeed = FMath::Max(MaxRestSpeed, Harness.Chassis().LinearVelocity.Size()); });

	AddInfo(FString::Printf(TEXT("%.2f m/s after %.1f s, max wheel speed %.1f rad/s, stopped %.1f m from the start"),
		TopSpeed, DriveSeconds, Driving.MaxWheelSpeed, FVector2D(StoppedAt - StartPosition).Size()));
	TestTrue(TEXT("everything stays finite"), Driving.bFinite && Stopping.bFinite);
	TestTrue(FString::Printf(TEXT("pulls away (%g m/s)"), TopSpeed), TopSpeed > 0.5);
	// When the car stops, the body pitches back on its springs and the tyres give a little at the ground, smoothly and
	// once: a couple of mm/s at most. Jitter would show as per-tick reversals far beyond that.
	TestTrue(FString::Printf(TEXT("never slows down while accelerating (worst %g m/s in a tick)"), Driving.WorstReversal), Driving.WorstReversal < 5.0e-4);
	TestTrue(FString::Printf(TEXT("never speeds up while braking (worst %g m/s in a tick)"), Stopping.WorstReversal), Stopping.WorstReversal < 5.0e-4);
	TestTrue(FString::Printf(TEXT("never rolls backwards (%g m/s)"), MinForward), MinForward > -5.0e-3);
	TestTrue(FString::Printf(TEXT("drives straight (side speed up to %g m/s, yaw rate up to %g rad/s)"), FMath::Max(Driving.MaxSideSpeed, Stopping.MaxSideSpeed), FMath::Max(Driving.MaxYawRate, Stopping.MaxYawRate)),
		FMath::Max(Driving.MaxSideSpeed, Stopping.MaxSideSpeed) < 1.0e-3 && FMath::Max(Driving.MaxYawRate, Stopping.MaxYawRate) < 1.0e-3);
	TestTrue(FString::Printf(TEXT("stands still once stopped (%g m/s)"), MaxRestSpeed), MaxRestSpeed < 1.0e-3);
}

void FGTODrivingSpec::Define()
{
	Describe("Pulling away and stopping", [this]()
	{
		It("creeps off with a touch of throttle and stops again without jitter or divergence", [this]()
		{
			PullAwayAndStop(0.05f, 2.0, 0.2f);
		});

		It("launches at full throttle with wheelspin and stops again without jitter or divergence", [this]()
		{
			PullAwayAndStop(1.f, 3.0, 0.5f);
		});
	});

	Describe("Pushed sideways at standstill", [this]()
	{
		It("comes to rest without rocking from side to side", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			FGTOHeadlessHarness Harness = SettledCar(Params, DroppedAbove(Params, FGTOFlatGround(), 0.0));
			// Settled; now start over from that pose with a sideways shove.
			FGTOChassisKinematics Start = Harness.Chassis();
			Start.LinearVelocity = FVector(0.0, 0.5, 0.0);
			FGTOHeadlessHarness Pushed = MakeAdapterLikeHarness(Params, FGTOFlatGround(), Start);

			double MinSideSpeed = 0.0;
			RunFor(Pushed, 2.0, [&]() { MinSideSpeed = FMath::Min(MinSideSpeed, GroundVelocity(Pushed.Chassis()).Y); });
			double MaxRestSpeed = 0.0;
			RunFor(Pushed, 1.0, [&]() { MaxRestSpeed = FMath::Max(MaxRestSpeed, Pushed.Chassis().LinearVelocity.Size()); });

			TestTrue(FString::Printf(TEXT("does not swing back (lowest side speed %g m/s)"), MinSideSpeed), MinSideSpeed > -0.01);
			TestTrue(FString::Printf(TEXT("at rest after 2 s (%g m/s)"), MaxRestSpeed), MaxRestSpeed < 1.0e-3);
		});
	});

	Describe("Steering right at 15 m/s", [this]()
	{
		It("turns right and leans its weight onto the outside wheels", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			const float Steer = 0.1f;
			const double KinematicYawRate = 15.0 * Steer * Params.Front.MaxSteerAngle / (Params.Front.PositionX - Params.Rear.PositionX); // v·δ/L, no slip
			FGTOChassisKinematics Start = DroppedAbove(Params, FGTOFlatGround(), 0.0);
			Start.LinearVelocity.X = 15.0;
			FGTOHeadlessHarness Harness = MakeAdapterLikeHarness(Params, FGTOFlatGround(), Start);
			RunFor(Harness, 1.0);

			FGTOInputPacket Packet;
			Packet.Steer = Steer;
			RunFor(Harness, 3.0, Packet);

			const FWheelStates& Wheels = Harness.State().Wheels;
			const double YawRate = Harness.Chassis().AngularVelocity.Z; // positive turns right
			const double Outside = Wheels[FrontLeft].Load + Wheels[RearLeft].Load;
			const double Inside = Wheels[FrontRight].Load + Wheels[RearRight].Load;
			AddInfo(FString::Printf(TEXT("yaw rate %.3f rad/s (kinematic %.3f), outside %.0f N, inside %.0f N, front slip angles %.2f %.2f deg"),
				YawRate, KinematicYawRate, Outside, Inside, FMath::RadiansToDegrees(Wheels[FrontLeft].SlipAngle), FMath::RadiansToDegrees(Wheels[FrontRight].SlipAngle)));

			// Below the kinematic rate, as the tyres need slip angles to corner, but well within reach of it.
			TestTrue(FString::Printf(TEXT("turns right steadily (%g rad/s)"), YawRate), YawRate > 0.5 * KinematicYawRate && YawRate < KinematicYawRate);
			TestTrue(FString::Printf(TEXT("outside wheels carry more (%g N vs %g N)"), Outside, Inside), Outside > Inside + 1000.0);
		});
	});

	Describe("Braking hard from 30 m/s without ABS", [this]()
	{
		It("locks all wheels without the wheel speed oscillating around zero", [this]()
		{
			const FGTOVehicleParams Params = MakeSportsCar();
			FGTOChassisKinematics Start = DroppedAbove(Params, FGTOFlatGround(), 0.0);
			Start.LinearVelocity.X = 30.0;
			FGTOHeadlessHarness Harness = MakeAdapterLikeHarness(Params, FGTOFlatGround(), Start);
			RunFor(Harness, 1.0); // wheels spin up to road speed and the car settles while rolling

			const double RollingWheelSpeed = Harness.State().Wheels[FrontLeft].AngularVelocity;
			const double RoadSpeed = BodyVelocity(Harness.Chassis()).X;
			TStaticArray<double, GTONumWheels> LockedAfter(InPlace, -1.0);
			double LowestWheelSpeed = 0.0;
			bool bSpunAgain = false;
			double Elapsed = 0.0;
			RunFor(Harness, 10.0, Controls(0.f, 1.f), [&]()
			{
				Elapsed += Harness.Dt();
				for (int32 Index = 0; Index < GTONumWheels; ++Index)
				{
					const double WheelSpeed = Harness.State().Wheels[Index].AngularVelocity;
					LowestWheelSpeed = FMath::Min(LowestWheelSpeed, WheelSpeed);
					if (LockedAfter[Index] < 0.0 && WheelSpeed == 0.0)
					{
						LockedAfter[Index] = Elapsed;
					}
					bSpunAgain |= LockedAfter[Index] >= 0.0 && WheelSpeed != 0.0;
				}
			});
			const double FinalSpeed = Harness.Chassis().LinearVelocity.Size();
			const double Heading = FMath::Abs(Harness.Chassis().Orientation.Rotator().Yaw);

			AddInfo(FString::Printf(TEXT("rolling at %.2f m/s, wheels at %.1f rad/s, locked after %.3f %.3f %.3f %.3f s"), RoadSpeed, RollingWheelSpeed, LockedAfter[0], LockedAfter[1], LockedAfter[2], LockedAfter[3]));
			TestTrue(TEXT("wheels rolled at road speed before braking"), FMath::IsNearlyEqual(RollingWheelSpeed * Params.Front.WheelRadius, RoadSpeed, 0.05));
			for (int32 Index = 0; Index < GTONumWheels; ++Index)
			{
				TestTrue(FString::Printf(TEXT("wheel %d locks within 0.3 s (%g s)"), Index, LockedAfter[Index]), LockedAfter[Index] >= 0.0 && LockedAfter[Index] < 0.3);
			}
			TestTrue(TEXT("locked wheels stay locked"), !bSpunAgain);
			TestTrue(FString::Printf(TEXT("no wheel ever turns backwards (%g rad/s)"), LowestWheelSpeed), LowestWheelSpeed >= 0.0);
			TestTrue(FString::Printf(TEXT("car stops (%g m/s)"), FinalSpeed), FinalSpeed < 1.0e-3);
			TestTrue(FString::Printf(TEXT("and keeps its heading (%g deg)"), Heading), Heading < 1.0);
		});
	});
}

#endif
