#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Vehicle/GTOVehiclePawn.h"
#include "Vehicle/GTOUnits.h"
#include "Vehicle/GTOVehicleSimComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// In-engine smoke tests on the flat test map. They check only that adapter and Chaos work together (async tick,
// wheel casts, forces), watching the car through its sim snapshots, with rough tolerances.
namespace
{
	const TCHAR* FlatTestMap = TEXT("/Game/Maps/FlatTest");

	constexpr double TimeoutSeconds = 60.0;

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
			const double Weight = Params.Mass * Gravity;
			// Statics: the front axle carries the weight share of the rear axle's distance from the centre of mass.
			const double FrontWeightShare = -Params.Rear.PositionX / (Params.Front.PositionX - Params.Rear.PositionX);
			const auto Load = [&State](EGTOWheel Wheel) { return State.Wheels[static_cast<int32>(Wheel)].Load; };
			const double FrontLoad = Load(EGTOWheel::FrontLeft) + Load(EGTOWheel::FrontRight);
			const double TotalLoad = FrontLoad + Load(EGTOWheel::RearLeft) + Load(EGTOWheel::RearRight);

			Test.AddInfo(FString::Printf(TEXT("loads FL %.0f FR %.0f RL %.0f RR %.0f N, weight %.0f N, front share %.4f, max speed %g m/s, max angular speed %g rad/s, height %.4f m, %.1f physics steps/s"),
				Load(EGTOWheel::FrontLeft), Load(EGTOWheel::FrontRight), Load(EGTOWheel::RearLeft), Load(EGTOWheel::RearRight),
				Weight, FrontLoad / TotalLoad, MaxSpeed, MaxAngularSpeed, State.Chassis.Position.Z, Sim()->GetTicksPerSecond()));
			for (int32 Index = 0; Index < GTONumWheels; ++Index)
			{
				Test.TestTrue(FString::Printf(TEXT("wheel %d carries load (%g N)"), Index, State.Wheels[Index].Load), State.Wheels[Index].Load > 0.0);
			}
			const double FixedRate = 1.0 / UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
			Test.TestNearlyEqual(TEXT("physics steps per second match the fixed step"), Sim()->GetTicksPerSecond(), FixedRate, 0.05 * FixedRate);
			Test.TestNearlyEqual(TEXT("wheels carry the weight (N)"), TotalLoad, Weight, 0.03 * Weight);
			Test.TestNearlyEqual(TEXT("front share of the load"), FrontLoad / TotalLoad, FrontWeightShare, 0.015);
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

	template <typename RunType>
	void PlayFlatTestMap(FAutomationTestBase& Test)
	{
		AutomationOpenMap(FlatTestMap);

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
	PlayFlatTestMap<FCarStandsStillRun>(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGTOResetPutsCarOnStartTest, "GTO.Smoke.ResetPutsCarOnStart", SmokeTestFlags)

bool FGTOResetPutsCarOnStartTest::RunTest(const FString& Parameters)
{
	PlayFlatTestMap<FResetRun>(*this);
	return true;
}

#endif
