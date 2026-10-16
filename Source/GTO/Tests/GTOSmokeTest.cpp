#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GTOHeadlessHarness.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Track/GTOStartFinishLine.h"
#include "Track/GTOTestTrack.h"
#include "Vehicle/GTOVehiclePawn.h"
#include "Vehicle/GTOUnits.h"
#include "Vehicle/GTOVehicleSimComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// In-engine smoke tests. On the flat test map they check only that adapter and Chaos work together (async tick,
// wheel casts, forces, input packets); on the test track, that the car starts at the line, feels the surfaces, rides
// over curbs and is timed. They watch the car through its sim snapshots, with rough tolerances.
namespace
{
	const TCHAR* FlatTestMap = TEXT("/Game/Maps/FlatTest");
	const TCHAR* TestTrackMap = TEXT("/Game/Maps/TestTrack");

	constexpr double TimeoutSeconds = 60.0;

	/** Static wheel loads of the car on flat ground, settled in the headless harness stepped like the adapter. */
	TStaticArray<double, GTONumWheels> HarnessStaticLoads(const FGTOVehicleParams& Params, double FixedStep, int32 NumSubsteps)
	{
		const FGTOFlatGround Ground;
		FGTOChassisKinematics Start;
		Start.Position.Z = Ground.Height + Params.FullDroopHeight();
		FGTOHeadlessHarness Harness(Params, Ground, Start, FixedStep, NumSubsteps);
		for (int32 Tick = 0; Tick < FMath::RoundToInt32(5.0 / FixedStep); ++Tick)
		{
			Harness.Tick();
		}

		TStaticArray<double, GTONumWheels> Loads;
		for (int32 Index = 0; Index < GTONumWheels; ++Index)
		{
			Loads[Index] = Harness.State().Wheels[Index].Load;
		}
		return Loads;
	}

	/** Finds the player's car once the map plays and measures time in sim ticks. Update() returns true when done. */
	class FSmokeRun
	{
	public:
		explicit FSmokeRun(FAutomationTestBase& InTest) : Test(InTest) {}
		virtual ~FSmokeRun() = default;

		bool Update()
		{
			if (FPlatformTime::Seconds() - StartWallTime > TimeoutSeconds)
			{
				Test.AddError(FString::Printf(TEXT("timed out after %g s (car found: %d)"), TimeoutSeconds, Vehicle.IsValid()));
				return true;
			}
			if (!Vehicle.IsValid())
			{
				FindVehicle();
				return false;
			}
			return Step(Sim()->GetSimulatedTicks() - FirstTick);
		}

	protected:
		/** Called every frame with the sim ticks since the car was found. */
		virtual bool Step(uint32 Ticks) = 0;

		static uint32 TicksFor(double Seconds)
		{
			return FMath::RoundToInt32(Seconds / UPhysicsSettings::Get()->AsyncFixedTimeStepSize);
		}

		const UGTOVehicleSimComponent* Sim() const { return Vehicle->GetSim(); }

		FAutomationTestBase& Test;
		TWeakObjectPtr<AGTOVehiclePawn> Vehicle;
		FTransform Start;
		double Gravity = 0.0;

	private:
		void FindVehicle()
		{
			UWorld* World = AutomationCommon::GetAnyGameWorld();
			if (!World)
			{
				return;
			}
			TActorIterator<AGTOVehiclePawn> VehicleIt(World);
			TActorIterator<APlayerStart> StartIt(World);
			if (VehicleIt && VehicleIt->GetSim() && StartIt)
			{
				Vehicle = *VehicleIt;
				Start = StartIt->GetActorTransform();
				FirstTick = Vehicle->GetSim()->GetSimulatedTicks();
				Gravity = FMath::Abs(World->GetGravityZ()) / GTOUnits::CmPerM;
			}
		}

		const double StartWallTime = FPlatformTime::Seconds();
		uint32 FirstTick = 0;
	};

	/** Spawn → car stands still. */
	class FCarStandsStillRun : public FSmokeRun
	{
	public:
		using FSmokeRun::FSmokeRun;

	private:
		static constexpr double SettleSeconds = 3.0;
		static constexpr double MeasureSeconds = 1.0;

		virtual bool Step(uint32 Ticks) override
		{
			const FGTOVehicleState State = Sim()->GetLatestState();
			if (Ticks < TicksFor(SettleSeconds))
			{
				MeasureStart = State.Chassis.Position;
				return false;
			}

			MaxSpeed = FMath::Max(MaxSpeed, State.Chassis.LinearVelocity.Size());
			MaxAngularSpeed = FMath::Max(MaxAngularSpeed, State.Chassis.AngularVelocity.Size());
			if (Ticks < TicksFor(SettleSeconds + MeasureSeconds))
			{
				return false;
			}

			Check(State);
			return true;
		}

		void Check(const FGTOVehicleState& State)
		{
			const FGTOVehicleParams Params = Sim()->GetParams();
			const double FixedStep = UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
			const TStaticArray<double, GTONumWheels> Expected = HarnessStaticLoads(Params, FixedStep, Sim()->NumSubsteps);
			const double Weight = Params.Mass * Gravity;
			const auto Load = [&State](EGTOWheel Wheel) { return State.Wheels[static_cast<int32>(Wheel)].Load; };
			const double FrontLoad = Load(EGTOWheel::FrontLeft) + Load(EGTOWheel::FrontRight);
			const double TotalLoad = FrontLoad + Load(EGTOWheel::RearLeft) + Load(EGTOWheel::RearRight);

			Test.AddInfo(FString::Printf(TEXT("loads FL %.0f FR %.0f RL %.0f RR %.0f N (harness %.0f %.0f %.0f %.0f N), weight %.0f N, front share %.4f, max speed %g m/s, max angular speed %g rad/s, height %.4f m, %.1f physics steps/s"),
				Load(EGTOWheel::FrontLeft), Load(EGTOWheel::FrontRight), Load(EGTOWheel::RearLeft), Load(EGTOWheel::RearRight),
				Expected[0], Expected[1], Expected[2], Expected[3],
				Weight, FrontLoad / TotalLoad, MaxSpeed, MaxAngularSpeed, State.Chassis.Position.Z, Sim()->GetTicksPerSecond()));
			for (int32 Index = 0; Index < GTONumWheels; ++Index)
			{
				// 1 % of the weight: Chaos and the harness integrate differently and use slightly different gravity.
				Test.TestNearlyEqual(FString::Printf(TEXT("wheel %d load matches the harness (N)"), Index), State.Wheels[Index].Load, Expected[Index], 0.01 * Weight);
			}
			const double FixedRate = 1.0 / FixedStep;
			Test.TestNearlyEqual(TEXT("physics steps per second match the fixed step"), Sim()->GetTicksPerSecond(), FixedRate, 0.05 * FixedRate);
			Test.TestNearlyEqual(TEXT("wheels carry the weight (N)"), TotalLoad, Weight, 0.03 * Weight);
			Test.TestTrue(FString::Printf(TEXT("max speed in the last second (%g m/s) < 1 cm/s"), MaxSpeed), MaxSpeed < 0.01);
			Test.TestTrue(FString::Printf(TEXT("max angular speed in the last second (%g rad/s) < 0.01 rad/s"), MaxAngularSpeed), MaxAngularSpeed < 0.01);
			Test.TestTrue(TEXT("no movement in the last second"), FVector::Dist(MeasureStart, State.Chassis.Position) < 0.01);
		}

		FVector MeasureStart = FVector::ZeroVector;
		double MaxSpeed = 0.0;
		double MaxAngularSpeed = 0.0;
	};

	/** Car thrown off the start point, then reset → stands on the start point, level and facing the start direction. */
	class FResetRun : public FSmokeRun
	{
	public:
		using FSmokeRun::FSmokeRun;

	private:
		static constexpr double DisplaceAfterSeconds = 1.0;
		static constexpr double ResetAfterSeconds = 1.5;
		static constexpr double SettleSeconds = 3.0;

		virtual bool Step(uint32 Ticks) override
		{
			if (!bDisplaced && Ticks >= TicksFor(DisplaceAfterSeconds))
			{
				// 5 m aside, 1 m up, turned and tilted: the car falls and lands somewhere else.
				UPrimitiveComponent* Body = CastChecked<UPrimitiveComponent>(Vehicle->GetRootComponent());
				Body->SetWorldLocationAndRotation(Start.GetLocation() + FVector(500.0, 300.0, 100.0), FRotator(5.0, 70.0, 10.0), false, nullptr, ETeleportType::ResetPhysics);
				bDisplaced = true;
			}
			if (!bReset && Ticks >= TicksFor(ResetAfterSeconds))
			{
				Vehicle->ResetToStart();
				bReset = true;
			}
			if (Ticks < TicksFor(ResetAfterSeconds + SettleSeconds))
			{
				return false;
			}

			const FGTOChassisKinematics Chassis = Sim()->GetLatestState().Chassis;
			const FRotator Rotation = Chassis.Orientation.Rotator();
			const double Offset = FVector2D(Chassis.Position * GTOUnits::CmPerM - Start.GetLocation()).Size();
			const double YawError = FMath::Abs(FRotator::NormalizeAxis(Rotation.Yaw - Start.Rotator().Yaw));

			Test.AddInfo(FString::Printf(TEXT("after reset: %.2f cm from the start point, rotation %s, speed %g m/s"), Offset, *Rotation.ToString(), Chassis.LinearVelocity.Size()));
			Test.TestTrue(FString::Printf(TEXT("on the start point (%.2f cm off) within 2 cm"), Offset), Offset < 2.0);
			Test.TestTrue(FString::Printf(TEXT("facing the start direction (%.3f° off) within 0.5°"), YawError), YawError < 0.5);
			Test.TestTrue(FString::Printf(TEXT("level (pitch %.3f°, roll %.3f°) within 0.5°"), Rotation.Pitch, Rotation.Roll), FMath::Abs(Rotation.Pitch) < 0.5 && FMath::Abs(Rotation.Roll) < 0.5);
			Test.TestTrue(FString::Printf(TEXT("at rest (%g m/s)"), Chassis.LinearVelocity.Size()), Chassis.LinearVelocity.Size() < 0.01);
			return true;
		}

		bool bDisplaced = false;
		bool bReset = false;
	};

	/** Settled car → throttle → full brake: drives off straight ahead and stops again. */
	class FDrivesAndStopsRun : public FSmokeRun
	{
	public:
		using FSmokeRun::FSmokeRun;

	private:
		static constexpr double SettleSeconds = 2.0;
		static constexpr double DriveSeconds = 3.0;
		static constexpr double BrakeSeconds = 5.0;

		virtual bool Step(uint32 Ticks) override
		{
			FGTOInputPacket Packet;
			const FGTOChassisKinematics Chassis = Sim()->GetLatestState().Chassis;
			if (Ticks < TicksFor(SettleSeconds))
			{
				SettledAt = Chassis.Position;
			}
			else if (Ticks < TicksFor(SettleSeconds + DriveSeconds))
			{
				Packet.Throttle = 0.6f;
				TopSpeed = FMath::Max(TopSpeed, Chassis.LinearVelocity.Size());
			}
			else if (Ticks < TicksFor(SettleSeconds + DriveSeconds + BrakeSeconds))
			{
				Packet.Brake = 1.f;
			}
			else
			{
				Vehicle->SetScriptedInput({});
				Check(Chassis);
				return true;
			}
			Vehicle->SetScriptedInput(Packet);
			return false;
		}

		void Check(const FGTOChassisKinematics& Chassis)
		{
			const FVector Heading = Start.GetRotation().GetForwardVector();
			const FVector Travel = Chassis.Position - SettledAt; // m
			const double Ahead = FVector::DotProduct(Travel, Heading);
			const double Aside = FVector2D(Travel - Heading * Ahead).Size();
			const double YawError = FMath::Abs(FRotator::NormalizeAxis(Chassis.Orientation.Rotator().Yaw - Start.Rotator().Yaw));

			Test.AddInfo(FString::Printf(TEXT("top speed %.2f m/s, stopped %.2f m ahead and %.3f m aside, heading off by %.2f deg, speed %g m/s"), TopSpeed, Ahead, Aside, YawError, Chassis.LinearVelocity.Size()));
			Test.TestTrue(FString::Printf(TEXT("drives off (%.2f m/s)"), TopSpeed), TopSpeed > 5.0);
			Test.TestTrue(FString::Printf(TEXT("ends up ahead of where it started (%.2f m)"), Ahead), Ahead > 5.0);
			Test.TestTrue(FString::Printf(TEXT("goes straight (%.3f m aside, %.2f deg)"), Aside, YawError), Aside < 0.5 && YawError < 2.0);
			Test.TestTrue(FString::Printf(TEXT("stops (%g m/s)"), Chassis.LinearVelocity.Size()), Chassis.LinearVelocity.Size() < 0.05);
		}

		FVector SettledAt = FVector::ZeroVector;
		double TopSpeed = 0.0;
	};

	/** Friction of one of the test track's physical materials, e.g. "PM_Grass". */
	double SurfaceFriction(const TCHAR* Name)
	{
		const UPhysicalMaterial* Material = LoadObject<UPhysicalMaterial>(nullptr, *FString::Printf(TEXT("/Game/Physics/%s.%s"), Name, Name));
		return Material ? Material->Friction : -1.0;
	}

	/** A run on the test track, with its track and start-finish line at hand. */
	class FTrackRun : public FSmokeRun
	{
	public:
		using FSmokeRun::FSmokeRun;

	protected:
		const AGTOTestTrack* Track() const
		{
			const TActorIterator<AGTOTestTrack> It(Vehicle->GetWorld());
			return It ? *It : nullptr;
		}

		FGTOTimingGate Gate() const
		{
			const TActorIterator<AGTOStartFinishLine> It(Vehicle->GetWorld());
			return It ? It->GetTimingGate() : FGTOTimingGate();
		}

		/** True if the map has the track; reports it missing otherwise. */
		bool CheckTrack()
		{
			if (!Track())
			{
				Test.AddError(TEXT("no test track in the map"));
				return false;
			}
			return true;
		}

		void ResetOnTrack(double Distance, double Lateral)
		{
			Vehicle->ResetTo(Track()->GetTrackTransform(Distance, Lateral));
		}

		/** Surface friction under each wheel, as the sim saw it last. */
		TStaticArray<double, GTONumWheels> WheelFriction() const
		{
			const FGTOVehicleState State = Sim()->GetLatestState();
			TStaticArray<double, GTONumWheels> Result;
			for (int32 Index = 0; Index < GTONumWheels; ++Index)
			{
				Result[Index] = State.Wheels[Index].SurfaceFriction;
			}
			return Result;
		}
	};

	/** Spawn → stands on the road just behind the start-finish line, facing it. */
	class FStartsAtTheLineRun : public FTrackRun
	{
	public:
		using FTrackRun::FTrackRun;

	private:
		virtual bool Step(uint32 Ticks) override
		{
			if (Ticks < TicksFor(2.0))
			{
				return false;
			}
			if (!CheckTrack())
			{
				return true;
			}

			const FGTOChassisKinematics Chassis = Sim()->GetLatestState().Chassis;
			const FGTOTimingGate Line = Gate();
			const FVector Offset = Chassis.Position - Line.Centre;
			const double Ahead = FVector::DotProduct(Offset, Line.Forward);
			const double Aside = FVector::DotProduct(Offset, FVector::CrossProduct(FVector::UpVector, Line.Forward));
			const double YawError = FMath::Abs(FRotator::NormalizeAxis(Chassis.Orientation.Rotator().Yaw - Line.Forward.Rotation().Yaw));
			const TStaticArray<double, GTONumWheels> Friction = WheelFriction();

			Test.AddInfo(FString::Printf(TEXT("%.2f m ahead of the line, %.2f m aside, heading off by %.2f deg, wheel grip %.2f %.2f %.2f %.2f"), Ahead, Aside, YawError, Friction[0], Friction[1], Friction[2], Friction[3]));
			Test.TestTrue(FString::Printf(TEXT("just behind the line (%.2f m)"), Ahead), Ahead < 0.0 && Ahead > -10.0);
			Test.TestTrue(FString::Printf(TEXT("in the middle of the road (%.2f m aside)"), Aside), FMath::Abs(Aside) < 0.5);
			Test.TestTrue(FString::Printf(TEXT("facing the line (%.2f deg off)"), YawError), YawError < 1.0);
			for (int32 Index = 0; Index < GTONumWheels; ++Index)
			{
				Test.TestEqual(FString::Printf(TEXT("wheel %d on asphalt"), Index), Friction[Index], SurfaceFriction(TEXT("PM_Asphalt")), 1.0e-3);
			}
			Test.TestFalse(TEXT("no lap running before the line"), Vehicle->GetLapTimer() && Vehicle->GetLapTimer()->GetCurrentLapTime().IsSet());
			return true;
		}
	};

	/** Drives over the line → a lap runs; reset → the lap is dropped. */
	class FLapTimingRun : public FTrackRun
	{
	public:
		using FTrackRun::FTrackRun;

	private:
		virtual bool Step(uint32 Ticks) override
		{
			const FGTOLapTimer* Timer = Vehicle->GetLapTimer();
			if (!Timer)
			{
				Test.AddError(TEXT("the car has no lap timer"));
				return true;
			}
			FGTOInputPacket Packet;
			if (Ticks >= TicksFor(1.0) && Ticks < TicksFor(5.0))
			{
				Packet.Throttle = 0.5f;
			}
			if (Ticks < TicksFor(5.0))
			{
				Vehicle->SetScriptedInput(Packet);
				return false;
			}
			if (!bReset)
			{
				LapAfterCrossing = Timer->GetCurrentLapTime();
				Vehicle->SetScriptedInput(Packet);
				Vehicle->ResetToStart();
				bReset = true;
				return false;
			}

			Vehicle->SetScriptedInput({});
			Test.AddInfo(FString::Printf(TEXT("lap time after crossing the line %.3f s"), LapAfterCrossing.Get(-1.0)));
			Test.TestTrue(TEXT("crossing the line starts a lap"), LapAfterCrossing.IsSet() && *LapAfterCrossing > 0.0 && *LapAfterCrossing < 4.0);
			Test.TestFalse(TEXT("the reset drops the lap"), Timer->GetCurrentLapTime().IsSet());
			Test.TestFalse(TEXT("and counts no lap"), Timer->GetLastLapTime().IsSet() || Timer->GetBestLapTime().IsSet());
			return true;
		}

		bool bReset = false;
		TOptional<double> LapAfterCrossing;
	};

	/** Put on the grass, then astride the curb → the wheels report the friction of what they stand on. */
	class FSurfaceGripRun : public FTrackRun
	{
	public:
		using FTrackRun::FTrackRun;

	private:
		static constexpr double SettleSeconds = 1.5;

		virtual bool Step(uint32 Ticks) override
		{
			if (Phase == 0)
			{
				if (!CheckTrack())
				{
					return true;
				}
				// Half way down the main straight, 25 m off the road.
				ResetOnTrack(0.5 * Track()->GetLayout().GetSegments()[0].Length, 25.0);
				PhaseStart = Ticks;
				Phase = 1;
				return false;
			}
			if (Ticks - PhaseStart < TicksFor(SettleSeconds))
			{
				return false;
			}
			if (Phase == 1)
			{
				const TStaticArray<double, GTONumWheels> Friction = WheelFriction();
				for (int32 Index = 0; Index < GTONumWheels; ++Index)
				{
					Test.TestEqual(FString::Printf(TEXT("on the grass: wheel %d"), Index), Friction[Index], SurfaceFriction(TEXT("PM_Grass")), 1.0e-3);
				}

				// In the middle of the first corner, a right-hander, with the left wheels on the outside curb.
				const FGTOTrackLayout& Layout = Track()->GetLayout();
				const FGTOTrackCrossSection& Section = Layout.GetCrossSection();
				const double OnCurb = Section.HalfWidth + 0.5 * (Section.CurbRise + Section.CurbWidth);
				ResetOnTrack(Layout.GetSegmentStart(1) + 0.5 * Layout.GetSegments()[1].Length, -OnCurb + 0.5 * Sim()->GetParams().Front.TrackWidth);
				PhaseStart = Ticks;
				Phase = 2;
				return false;
			}

			const TStaticArray<double, GTONumWheels> Friction = WheelFriction();
			Test.AddInfo(FString::Printf(TEXT("astride the curb: grip %.2f %.2f %.2f %.2f"), Friction[0], Friction[1], Friction[2], Friction[3]));
			for (int32 Index = 0; Index < GTONumWheels; ++Index)
			{
				const TCHAR* Surface = GTOIsLeft(static_cast<EGTOWheel>(Index)) ? TEXT("PM_Curb") : TEXT("PM_Asphalt");
				Test.TestEqual(FString::Printf(TEXT("astride the curb: wheel %d on %s"), Index, Surface), Friction[Index], SurfaceFriction(Surface), 1.0e-3);
			}
			Test.TestTrue(TEXT("less grip on the curb than on asphalt, less again on the grass"),
				SurfaceFriction(TEXT("PM_Grass")) < SurfaceFriction(TEXT("PM_Curb")) && SurfaceFriction(TEXT("PM_Curb")) < SurfaceFriction(TEXT("PM_Asphalt")));
			return true;
		}

		int32 Phase = 0;
		uint32 PhaseStart = 0;
	};

	/**
	 * Goes straight on at the first corner under throttle → runs wide over the outside curb onto the grass at a shallow
	 * angle, as when missing a corner. The curb must neither throw the car up nor snag it.
	 */
	class FRunsWideOverACurbRun : public FTrackRun
	{
	public:
		using FTrackRun::FTrackRun;

	private:
		static constexpr double GiveUpSeconds = 12.0;
		static constexpr double OnGrassSeconds = 1.0;

		virtual bool Step(uint32 Ticks) override
		{
			if (!PlacedAt)
			{
				if (!CheckTrack())
				{
					return true;
				}
				ResetOnTrack(Track()->GetLayout().GetSegmentStart(1) - 20.0, 0.0);
				PlacedAt = Ticks;
				return false;
			}

			const uint32 Since = Ticks - *PlacedAt;
			FGTOInputPacket Packet;
			Packet.Throttle = Since < TicksFor(1.0) ? 0.f : 0.7f; // settle, then straight on
			Vehicle->SetScriptedInput(Packet);
			Watch(Since);

			const bool bDone = GrassAt && Since - *GrassAt >= TicksFor(OnGrassSeconds);
			if (!bDone && Since < TicksFor(GiveUpSeconds))
			{
				return false;
			}
			Vehicle->SetScriptedInput({});
			Check();
			return true;
		}

		void Watch(uint32 Since)
		{
			const FGTOVehicleState State = Sim()->GetLatestState();
			bool bOnCurb = false;
			bool bAllOnGrass = true;
			for (const FGTOWheelState& Wheel : State.Wheels)
			{
				bOnCurb |= FMath::IsNearlyEqual(Wheel.SurfaceFriction, SurfaceFriction(TEXT("PM_Curb")), 1.0e-3);
				bAllOnGrass &= FMath::IsNearlyEqual(Wheel.SurfaceFriction, SurfaceFriction(TEXT("PM_Grass")), 1.0e-3);
			}
			const double Speed = State.Chassis.LinearVelocity.Size();
			if (bOnCurb && !CurbAt)
			{
				CurbAt = Since;
				SpeedAtCurb = Speed;
			}
			if (!CurbAt)
			{
				return;
			}
			if (bAllOnGrass && !GrassAt)
			{
				GrassAt = Since;
			}
			MaxVerticalSpeed = FMath::Max(MaxVerticalSpeed, FMath::Abs(State.Chassis.LinearVelocity.Z));
			MinSpeed = FMath::Min(MinSpeed, Speed);
			for (const FGTOWheelState& Wheel : State.Wheels)
			{
				// Contact, not load: the load of the massless wheels may dip to zero for a few ticks while a strut extends
				// fast off the curb's edge, with the tyre still on the ground.
				bWheelInAir |= Wheel.SurfaceFriction <= 0.0;
			}
		}

		void Check()
		{
			Test.AddInfo(FString::Printf(TEXT("on the curb at %.2f m/s, all wheels on the grass %.2f s later; max vertical speed %.3f m/s, slowest %.2f m/s"),
				SpeedAtCurb, CurbAt && GrassAt ? (*GrassAt - *CurbAt) * UPhysicsSettings::Get()->AsyncFixedTimeStepSize : -1.0, MaxVerticalSpeed, MinSpeed));
			Test.TestTrue(TEXT("ran over the curb"), CurbAt.IsSet());
			Test.TestTrue(TEXT("and on onto the grass"), GrassAt.IsSet());
			Test.TestTrue(FString::Printf(TEXT("reached the curb at speed (%.1f m/s)"), SpeedAtCurb), SpeedAtCurb > 10.0);
			Test.TestTrue(FString::Printf(TEXT("not thrown up (vertical speed %.3f m/s)"), MaxVerticalSpeed), MaxVerticalSpeed < 0.3);
			Test.TestFalse(TEXT("no wheel left the ground"), bWheelInAir);
			Test.TestTrue(FString::Printf(TEXT("not snagged (slowest %.2f m/s, from %.2f m/s)"), MinSpeed, SpeedAtCurb), MinSpeed > 0.9 * SpeedAtCurb);
		}

		TOptional<uint32> PlacedAt;
		TOptional<uint32> CurbAt;
		TOptional<uint32> GrassAt;
		double SpeedAtCurb = 0.0;
		double MinSpeed = TNumericLimits<double>::Max();
		double MaxVerticalSpeed = 0.0;
		bool bWheelInAir = false;
	};

	template <typename RunType>
	void PlayMap(FAutomationTestBase& Test, const TCHAR* Map)
	{
		AutomationOpenMap(Map);

		TSharedRef<RunType> Run = MakeShared<RunType>(Test);
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Run]() { return Run->Update(); }));

#if WITH_EDITOR
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
		{
			if (GIsEditor && GEditor)
			{
				GEditor->RequestEndPlayMap();
			}
			return true;
		}));
#endif
	}
}

constexpr EAutomationTestFlags SmokeTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTOCarStandsStillTest, "GTO.Smoke.CarStandsStill", SmokeTestFlags)

bool FGTOCarStandsStillTest::RunTest(const FString& Parameters)
{
	PlayMap<FCarStandsStillRun>(*this, FlatTestMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTODrivesAndStopsTest, "GTO.Smoke.DrivesAndStops", SmokeTestFlags)

bool FGTODrivesAndStopsTest::RunTest(const FString& Parameters)
{
	PlayMap<FDrivesAndStopsRun>(*this, FlatTestMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTOResetPutsCarOnStartTest, "GTO.Smoke.ResetPutsCarOnStart", SmokeTestFlags)

bool FGTOResetPutsCarOnStartTest::RunTest(const FString& Parameters)
{
	PlayMap<FResetRun>(*this, FlatTestMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTOStartsAtTheLineTest, "GTO.Smoke.Track.StartsAtTheLine", SmokeTestFlags)

bool FGTOStartsAtTheLineTest::RunTest(const FString& Parameters)
{
	PlayMap<FStartsAtTheLineRun>(*this, TestTrackMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTOLapTimingTest, "GTO.Smoke.Track.LapTiming", SmokeTestFlags)

bool FGTOLapTimingTest::RunTest(const FString& Parameters)
{
	PlayMap<FLapTimingRun>(*this, TestTrackMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTOSurfaceGripTest, "GTO.Smoke.Track.SurfaceGrip", SmokeTestFlags)

bool FGTOSurfaceGripTest::RunTest(const FString& Parameters)
{
	PlayMap<FSurfaceGripRun>(*this, TestTrackMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTORunsWideOverACurbTest, "GTO.Smoke.Track.RunsWideOverACurb", SmokeTestFlags)

bool FGTORunsWideOverACurbTest::RunTest(const FString& Parameters)
{
	PlayMap<FRunsWideOverACurbRun>(*this, TestTrackMap);
	return true;
}

#endif
