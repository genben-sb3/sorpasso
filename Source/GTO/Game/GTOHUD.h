#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GTOHUD.generated.h"

/** Developer telemetry drawn on the canvas: wheel loads, suspension travel, physics steps per second. */
UCLASS()
class GTO_API AGTOHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Telemetry")
	bool bShowTelemetry = true;

	void ToggleTelemetry() { bShowTelemetry = !bShowTelemetry; }

	virtual void DrawHUD() override;
};
