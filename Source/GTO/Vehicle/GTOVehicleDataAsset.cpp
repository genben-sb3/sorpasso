#include "Vehicle/GTOVehicleDataAsset.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FGTOAxleParams ToAxleParams(const FGTOAxleSettings& Settings)
	{
		FGTOAxleParams Axle;
		Axle.PositionX = Settings.PositionX;
		Axle.TrackWidth = Settings.TrackWidth;
		Axle.MountHeight = Settings.MountHeight;
		Axle.WheelRadius = Settings.WheelRadius;
		Axle.Suspension.SpringRate = Settings.Suspension.SpringRate;
		Axle.Suspension.BumpDamping = Settings.Suspension.BumpDamping;
		Axle.Suspension.ReboundDamping = Settings.Suspension.ReboundDamping;
		Axle.Suspension.MaxLength = Settings.Suspension.MaxLength;
		Axle.Suspension.BumpStopCompression = Settings.Suspension.BumpStopCompression;
		Axle.Suspension.BumpStopRate = Settings.Suspension.BumpStopRate;
		Axle.Suspension.AntiRollRate = Settings.Suspension.AntiRollRate;
		return Axle;
	}
}

UGTOVehicleDataAsset::UGTOVehicleDataAsset()
{
	// SportsCar mesh: wheel centres 2.642 m apart, tyre centres 1.61 m apart, tyre radius 0.393 m. The centre of mass
	// sits 1.189 m ahead of the rear axle (45:55 statics) and 0.50 m above the ground at rest. The mounts put both
	// wheel centres 0.107 m below the centre of mass, the template's front ride height, once the springs carry their
	// static share (6.4 cm compression front, 7.0 cm rear), so the car stands level.
	FGTOAxleSettings Axle;
	Axle.TrackWidth = 1.61;
	Axle.WheelRadius = 0.393;
	Axle.Suspension.MaxLength = 0.20;
	Axle.Suspension.BumpStopCompression = 0.11;
	Axle.Suspension.BumpStopRate = 5.0e5;

	Front = Axle;
	Front.PositionX = 1.453;
	Front.MountHeight = 0.030;
	Front.Suspension.SpringRate = 45000.0;
	Front.Suspension.BumpDamping = 2500.0;
	Front.Suspension.ReboundDamping = 4500.0;
	Front.Suspension.AntiRollRate = 20000.0;

	Rear = Axle;
	Rear.PositionX = -1.189;
	Rear.MountHeight = 0.023;
	Rear.Suspension.SpringRate = 50000.0;
	Rear.Suspension.BumpDamping = 2800.0;
	Rear.Suspension.ReboundDamping = 5000.0;
	Rear.Suspension.AntiRollRate = 10000.0;

	// The body mesh origin sits on the ground between the axles; BodyMeshOffset puts its wheel arches over the wheels.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassAsset(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelAsset(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
	BodyMesh = BodyAsset.Object;
	GlassMesh = GlassAsset.Object;
	WheelMesh = WheelAsset.Object;
}

FGTOVehicleParams UGTOVehicleDataAsset::ToParams() const
{
	FGTOVehicleParams Params;
	Params.Mass = Mass;
	Params.Inertia = Inertia;
	Params.Front = ToAxleParams(Front);
	Params.Rear = ToAxleParams(Rear);
	return Params;
}
