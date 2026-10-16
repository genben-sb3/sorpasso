#include "Game/GTOHUD.h"

#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Vehicle/GTOVehiclePawn.h"
#include "Vehicle/GTOVehicleSimComponent.h"

void AGTOHUD::DrawHUD()
{
	Super::DrawHUD();

	const AGTOVehiclePawn* Vehicle = Cast<AGTOVehiclePawn>(GetOwningPawn());
	if (!bShowTelemetry || !Vehicle || !Vehicle->GetSim())
	{
		return;
	}

	const UGTOVehicleSimComponent* Sim = Vehicle->GetSim();
	const FGTOVehicleState State = Sim->GetLatestState();
	const auto& Wheels = State.Wheels;

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Physics  %.0f steps/s  (fixed step %.0f/s)"), Sim->GetTicksPerSecond(), 1.0 / UPhysicsSettings::Get()->AsyncFixedTimeStepSize));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("Wheel     Load     Travel"));
	const TCHAR* Names[GTONumWheels] = { TEXT("FL"), TEXT("FR"), TEXT("RL"), TEXT("RR") };
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		Lines.Add(FString::Printf(TEXT("%s     %6.0f N   %5.1f mm"), Names[Index], Wheels[Index].Load, Wheels[Index].Compression * 1000.0));
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
