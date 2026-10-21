#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GTOVehicleDataAsset.generated.h"

class UStaticMesh;

namespace ultragravity { struct VehicleParams; }

/**
 * What Unreal needs to show a car: its meshes and where its vehicle file lies. The vehicle params (mass, geometry,
 * tyres, drivetrain, …) live only in the vehicle file (ADR 0010) and are read again on every reset.
 */
UCLASS(BlueprintType)
class GTO_API UGTOVehicleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGTOVehicleDataAsset();

	/** The vehicle file, relative to the project's Content folder. Its folder is staged into packaged builds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	FString VehicleFile = TEXT("VehicleFiles/sports_coupe.json");

	/**
	 * Half size of the collision box around the centre of mass (cm). Clear of the ground at rest, it only touches
	 * anything in a crash; mass and inertia come from the vehicle file and do not depend on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chassis", meta = (ClampMin = 1))
	FVector CollisionExtent = FVector(220.0, 85.0, 25.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UStaticMesh> BodyMesh;

	/** Attached to the body mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UStaticMesh> GlassMesh;

	/** Modelled for the left side; the right wheels are turned around. Scaled to the wheel radius of the vehicle file. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UStaticMesh> WheelMesh;

	/** Radius of the tyre as modelled in the wheel mesh (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display", meta = (ClampMin = 1))
	double WheelMeshRadius = 39.3;

	/**
	 * Origin of the body mesh in the vehicle file's frame (centre of the front axle on the ground in design position),
	 * with Unreal axes (cm).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FVector BodyMeshOrigin = FVector(-135.0, 0.0, 8.0);

	/** Full path of the vehicle file. */
	FString GetVehicleFilePath() const;

	/**
	 * Reads and checks the vehicle file. On an error returns false with a message naming the file, the field path and
	 * what is wrong.
	 */
	bool LoadVehicleParams(ultragravity::VehicleParams& OutParams, FString& OutError) const;
};
