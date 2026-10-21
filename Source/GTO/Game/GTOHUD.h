#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GTOHUD.generated.h"

class AGTOVehiclePawn;
class FGTOLapTimer;

/**
 * Canvas HUD: the session's lap times, speed, gear and traction control, and developer telemetry (driver input, engine,
 * and per wheel load, travel, spin, slip, tyre forces and surface grip) that can be switched off.
 */
UCLASS()
class GTO_API AGTOHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Telemetry")
	bool bShowTelemetry = true;

	void ToggleTelemetry() { bShowTelemetry = !bShowTelemetry; }

	virtual void DrawHUD() override;

private:
	void DrawLapTimes(const FGTOLapTimer& LapTimer);
	void DrawDriveInfo(const AGTOVehiclePawn& Vehicle);
	void DrawTelemetry(const AGTOVehiclePawn& Vehicle);
};
