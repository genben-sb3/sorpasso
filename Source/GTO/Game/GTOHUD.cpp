#include "Game/GTOHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Game/GTOLapTimer.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Vehicle/GTOVehiclePawn.h"
#include "Vehicle/GTOUnits.h"
#include "Vehicle/GTOVehicleSimComponent.h"

using namespace GTOUnits;

namespace
{
	/** m:ss.mmm, or dashes for no time. */
	FString FormatLapTime(const TOptional<double>& Seconds)
	{
		if (!Seconds)
		{
			return TEXT("-:--.---");
		}
		const int64 Milliseconds = FMath::RoundToInt64(*Seconds * 1000.0);
		return FString::Printf(TEXT("%lld:%02lld.%03lld"), Milliseconds / 60000, Milliseconds / 1000 % 60, Milliseconds % 1000);
	}

	/** R, N or the forward gear. */
	FString FormatGear(int32 Gear)
	{
		return Gear < 0 ? TEXT("R") : Gear == 0 ? TEXT("N") : FString::FromInt(Gear);
	}

	/** TC off, or its level. */
	FString FormatTractionControl(int32 Level)
	{
		return Level > 0 ? FString::Printf(TEXT("TC %d"), Level) : TEXT("TC off");
	}

	/** ABS on or off. */
	FString FormatAbs(bool bOn)
	{
		return bOn ? TEXT("ABS on") : TEXT("ABS off");
	}
}

void AGTOHUD::DrawHUD()
{
	Super::DrawHUD();

	const AGTOVehiclePawn* Vehicle = Cast<AGTOVehiclePawn>(GetOwningPawn());
	if (!Vehicle || !Vehicle->GetSim())
	{
		return;
	}
	if (const FGTOLapTimer* LapTimer = Vehicle->GetLapTimer())
	{
		DrawLapTimes(*LapTimer);
	}
	DrawDriveInfo(*Vehicle);
	if (bShowTelemetry)
	{
		DrawTelemetry(*Vehicle);
	}
}

void AGTOHUD::DrawLapTimes(const FGTOLapTimer& LapTimer)
{
	const TPair<const TCHAR*, TOptional<double>> Rows[] = {
		{ TEXT("LAP"), LapTimer.GetCurrentLapTime() },
		{ TEXT("LAST"), LapTimer.GetLastLapTime() },
		{ TEXT("BEST"), LapTimer.GetBestLapTime() },
	};
	UFont* Font = GEngine->GetMediumFont();
	constexpr float Scale = 1.5f;
	constexpr float RowHeight = 30.f;
	const float Right = Canvas->ClipX - 40.f;
	float Y = 40.f;
	for (const auto& [Label, Time] : Rows)
	{
		const FString Value = FormatLapTime(Time);
		float Width = 0.f, Height = 0.f;
		GetTextSize(Value, Width, Height, Font, Scale);
		DrawText(Label, FLinearColor(0.7f, 0.7f, 0.7f), Right - 260.f, Y, Font, Scale);
		DrawText(Value, FLinearColor::White, Right - Width, Y, Font, Scale);
		Y += RowHeight;
	}
}

void AGTOHUD::DrawDriveInfo(const AGTOVehiclePawn& Vehicle)
{
	const FGTOVehicleState State = Vehicle.GetSim()->GetLatestState();
	const FString Line = FString::Printf(TEXT("%3.0f km/h    %s    %s    %s"), FMath::Abs(State.Chassis.GetForwardSpeed()) * KmhPerMps, *FormatGear(State.Gear), *FormatTractionControl(Vehicle.GetTractionControl()), *FormatAbs(Vehicle.IsAbsOn()));
	UFont* Font = GEngine->GetMediumFont();
	constexpr float Scale = 1.5f;
	float Width = 0.f, Height = 0.f;
	GetTextSize(Line, Width, Height, Font, Scale);
	DrawText(Line, FLinearColor::White, Canvas->ClipX - 40.f - Width, Canvas->ClipY - 40.f - Height, Font, Scale);
}

void AGTOHUD::DrawTelemetry(const AGTOVehiclePawn& Vehicle)
{
	const UGTOVehicleSimComponent* Sim = Vehicle.GetSim();
	const FGTOVehicleState State = Sim->GetLatestState();
	const auto& Wheels = State.Wheels;

	const FGTODriverInput& Input = Vehicle.GetLastInput();
	const double Speed = State.Chassis.GetForwardSpeed();

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Physics  %.0f steps/s  (fixed step %.0f/s)"), Sim->GetTicksPerSecond(), 1.0 / UPhysicsSettings::Get()->AsyncFixedTimeStepSize));
	Lines.Add(FString::Printf(TEXT("Speed  %.1f km/h"), Speed * KmhPerMps));
	Lines.Add(FString::Printf(TEXT("Input  steer %+.2f  throttle %.2f  brake %.2f  handbrake %.2f  gear request %d  %s  %s"), Input.Steer, Input.Throttle, Input.Brake, Input.Handbrake, Input.GearRequest, *FormatTractionControl(Input.TractionControl), *FormatAbs(Input.bAbs)));
	Lines.Add(FString::Printf(TEXT("Engine  %.0f rpm  gear %s"), State.EngineSpeed * RpmPerRadPerSecond, *FormatGear(State.Gear)));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("Wheel     Load     Travel    Spin       Slip    Angle       Fx        Fy     Grip"));
	const TCHAR* Names[GTONumWheels] = { TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR") };
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FGTOWheelState& Wheel = Wheels[Index];
		Lines.Add(FString::Printf(TEXT("%s     %6.0f N  %5.1f mm  %6.1f/s  %+6.3f  %+6.2f deg  %+6.0f N  %+6.0f N  %4.2f"), Names[Index], Wheel.Load, Wheel.Compression * 1000.0,
			Wheel.AngularVelocity, Wheel.SlipRatio, FMath::RadiansToDegrees(Wheel.SlipAngle), Wheel.LongitudinalForce, Wheel.LateralForce, Wheel.SurfaceFriction));
	}

	const auto Load = [&Wheels](EGTOWheel Wheel) { return Wheels[static_cast<int32>(Wheel)].Load; };
	const double FrontLoad = Load(EGTOWheel::FrontLeft) + Load(EGTOWheel::FrontRight);
	const double TotalLoad = FrontLoad + Load(EGTOWheel::RearLeft) + Load(EGTOWheel::RearRight);
	if (TotalLoad > 0.0)
	{
		Lines.Add(TEXT(""));
		Lines.Add(FString::Printf(TEXT("Axle load  front %.1f %%  rear %.1f %%"), 100.0 * FrontLoad / TotalLoad, 100.0 * (1.0 - FrontLoad / TotalLoad)));
	}

	UFont* Font = GEngine->GetSmallFont();
	float Y = 40.f;
	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor::White, 40.f, Y, Font, 1.25f);
		Y += 18.f;
	}
}
