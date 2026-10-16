#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GTOVehicleParams.h"
#include "GTOVehicleDataAsset.generated.h"

class UStaticMesh;

/** Suspension of one axle, in SI units. See FGTOSuspensionParams. */
USTRUCT(BlueprintType)
struct FGTOSuspensionSettings
{
	GENERATED_BODY()

	/** N/m */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double SpringRate = 0.0;

	/** N·s/m, while compressing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double BumpDamping = 0.0;

	/** N·s/m, while extending */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double ReboundDamping = 0.0;

	/** m, strut length from mount to wheel centre at full droop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double MaxLength = 0.0;

	/** m, compression at which the bump stop engages */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double BumpStopCompression = 0.0;

	/** N/m, added on top of the spring beyond the bump stop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double BumpStopRate = 0.0;

	/** N/m, per metre of left/right compression difference */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = 0))
	double AntiRollRate = 0.0;
};

/** Geometry and suspension of one axle, in SI units relative to the centre of mass. See FGTOAxleParams. */
USTRUCT(BlueprintType)
struct FGTOAxleSettings
{
	GENERATED_BODY()

	/** m, axle ahead of the centre of mass (rear axle negative) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle")
	double PositionX = 0.0;

	/** m, between the tyre centres */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle", meta = (ClampMin = 0))
	double TrackWidth = 0.0;

	/** m, strut mount above the centre of mass */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle")
	double MountHeight = 0.0;

	/** m */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle", meta = (ClampMin = 0))
	double WheelRadius = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle")
	FGTOSuspensionSettings Suspension;
};

/**
 * Vehicle parameters, edited in the editor and translated into the plain FGTOVehicleParams of the sim core,
 * plus the meshes that show the car. The defaults are a light rear-drive sports car fitted to the SportsCar mesh
 * of the UE Vehicle Template.
 * Changes take effect when the car is reset.
 */
UCLASS(BlueprintType)
class GTO_API UGTOVehicleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGTOVehicleDataAsset();

	/** kg */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chassis", meta = (ClampMin = 1))
	double Mass = 1300.0;

	/** kg·m², principal moments about the centre of mass: roll (X), pitch (Y), yaw (Z) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chassis")
	FVector Inertia = FVector(500.0, 2000.0, 2200.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axles")
	FGTOAxleSettings Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axles")
	FGTOAxleSettings Rear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UStaticMesh> BodyMesh;

	/** Attached to the body mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UStaticMesh> GlassMesh;

	/** Modelled for the left side; the right wheels are turned around. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UStaticMesh> WheelMesh;

	/** Origin of the body mesh relative to the centre of mass, in body axes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FVector BodyMeshOffset = FVector(10.3, 0.0, -35.7);

	FGTOVehicleParams ToParams() const;
};
