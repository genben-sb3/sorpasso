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

/** One direction of the Magic Formula tyre. See FGTOTireCurveParams. */
USTRUCT(BlueprintType)
struct FGTOTireCurveSettings
{
	GENERATED_BODY()

	/** Peak force / load at nominal load (D) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double PeakFriction = 1.0;

	/** Exponent of the peak friction over relative load, between −1 and 0: more load, relatively less grip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = -0.99, ClampMax = 0))
	double LoadSensitivity = 0.0;

	/** Slip at the peak at nominal load: slip ratio longitudinally, rad laterally (0.12 rad = 6.9 deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0.001))
	double PeakSlip = 0.1;

	/** Relative change of the peak slip per relative load change */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire")
	double PeakSlipLoadSensitivity = 0.0;

	/** C, between 1 and 2: lower keeps more force when sliding */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 1.01, ClampMax = 1.99))
	double Shape = 1.5;

	/** E, at most 1: higher makes the peak flatter and the rise steeper */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMax = 1))
	double Curvature = 0.0;

	/** How fast this force fades as the other direction slips; 1 puts both peaks together on the friction circle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double CombinedSlipStiffness = 1.0;
};

/** Tyre of one axle. See FGTOTireParams. */
USTRUCT(BlueprintType)
struct FGTOTireSettings
{
	GENERATED_BODY()

	/** N, the load the curves are given for */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 1))
	double NominalLoad = 4000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire")
	FGTOTireCurveSettings Longitudinal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire")
	FGTOTireCurveSettings Lateral;

	/** m, rolling distance over which the slip angle builds up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double RelaxationLength = 0.4;

	/** m/s; below, the slip angle acts at once, which keeps standstill and parking damped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double RelaxationMinSpeed = 5.0;

	/** m/s; from here on the slip angle lags fully, blended in from RelaxationMinSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double RelaxationFullSpeed = 10.0;

	/** m/s; the slip denominator saturates smoothly towards 2/3 of it at standstill (VXLOW) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double SlipMinSpeed = 2.0;

	/** Rolling resistance force / load */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tire", meta = (ClampMin = 0))
	double RollingResistance = 0.015;
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

	/** kg·m², per wheel about its axle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle", meta = (ClampMin = 0.01))
	double WheelInertia = 1.0;

	/** rad, road wheel angle at full steering input; 0 for an axle that does not steer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle", meta = (ClampMin = 0))
	double MaxSteerAngle = 0.0;

	/** N·m per wheel at full brake */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle", meta = (ClampMin = 0))
	double BrakeTorque = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle")
	FGTOSuspensionSettings Suspension;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axle")
	FGTOTireSettings Tire;
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

	/**
	 * Half size of the collision box around the centre of mass (cm). Clear of the ground at rest, it only touches
	 * anything in a crash; mass and inertia above do not depend on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chassis", meta = (ClampMin = 1))
	FVector CollisionExtent = FVector(220.0, 85.0, 25.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axles")
	FGTOAxleSettings Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Axles")
	FGTOAxleSettings Rear;

	/** N·m at full throttle on both rear wheels together. Stand-in until the drivetrain arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = 0))
	double DriveTorque = 4000.0;

	/** W, caps the drive torque at wheel speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drive", meta = (ClampMin = 0))
	double DrivePower = 210000.0;

	/** N·m per rear wheel at full handbrake; the handbrake acts on the rear wheels only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brakes", meta = (ClampMin = 0))
	double HandbrakeTorque = 2000.0;

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
