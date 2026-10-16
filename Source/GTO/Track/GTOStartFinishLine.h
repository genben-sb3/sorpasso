#pragma once

#include "CoreMinimal.h"
#include "Game/GTOLapTimer.h"
#include "GameFramework/Actor.h"
#include "GTOStartFinishLine.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * Start-finish line: a painted stripe across the road and the timing gate around it, X along the driving direction.
 * The gate reaches GateMargin beyond the road on both sides, so a car with wheels on the verge still counts. Cars
 * time themselves against the gate from their sim positions (FGTOLapTimer); the box triggers no overlap events.
 */
UCLASS()
class GTO_API AGTOStartFinishLine : public AActor
{
	GENERATED_BODY()

public:
	AGTOStartFinishLine();

	/** Road width (m). */
	UPROPERTY(EditAnywhere, Category = "Line", meta = (ClampMin = 1))
	double Width = 12.0;

	/** How far the gate reaches beyond the road on each side (m). */
	UPROPERTY(EditAnywhere, Category = "Line", meta = (ClampMin = 0))
	double GateMargin = 3.0;

	UPROPERTY(EditAnywhere, Category = "Line")
	TObjectPtr<UMaterialInterface> StripeMaterial;

	/** Gate in world space, metres. */
	FGTOTimingGate GetTimingGate() const;

	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void ApplySettings();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Gate;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Stripe;
};
