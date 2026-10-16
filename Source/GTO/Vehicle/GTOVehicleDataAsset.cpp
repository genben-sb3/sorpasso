#include "Vehicle/GTOVehicleDataAsset.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FGTOTireCurveParams ToCurveParams(const FGTOTireCurveSettings& Settings)
	{
		FGTOTireCurveParams Curve;
		Curve.PeakFriction = Settings.PeakFriction;
		Curve.LoadSensitivity = Settings.LoadSensitivity;
		Curve.PeakSlip = Settings.PeakSlip;
		Curve.PeakSlipLoadSensitivity = Settings.PeakSlipLoadSensitivity;
		Curve.Shape = Settings.Shape;
		Curve.Curvature = Settings.Curvature;
		Curve.CombinedSlipStiffness = Settings.CombinedSlipStiffness;
		return Curve;
	}

	FGTOTireParams ToTireParams(const FGTOTireSettings& Settings)
	{
		FGTOTireParams Tire;
		Tire.NominalLoad = Settings.NominalLoad;
		Tire.Longitudinal = ToCurveParams(Settings.Longitudinal);
		Tire.Lateral = ToCurveParams(Settings.Lateral);
		Tire.RelaxationLength = Settings.RelaxationLength;
		Tire.RelaxationMinSpeed = Settings.RelaxationMinSpeed;
		Tire.RelaxationFullSpeed = Settings.RelaxationFullSpeed;
		Tire.SlipMinSpeed = Settings.SlipMinSpeed;
		Tire.RollingResistance = Settings.RollingResistance;
		return Tire;
	}

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
		Axle.WheelInertia = Settings.WheelInertia;
		Axle.MaxSteerAngle = Settings.MaxSteerAngle;
		Axle.BrakeTorque = Settings.BrakeTorque;
		Axle.Tire = ToTireParams(Settings.Tire);
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
	Axle.WheelInertia = 1.5;
	Axle.Suspension.MaxLength = 0.20;
	Axle.Suspension.BumpStopCompression = 0.11;
	Axle.Suspension.BumpStopRate = 5.0e5;

	// Road tyre at the spec's starting values: peak friction about 1.1, peak slip 0.1 and about 7°, relaxation length
	// 0.4 m. Laterally a low C and negative E keep most of the grip past the peak, so a drift can be held.
	FGTOTireSettings& Tire = Axle.Tire;
	Tire.NominalLoad = 3200.0;
	Tire.Longitudinal.PeakFriction = 1.1;
	Tire.Longitudinal.LoadSensitivity = -0.15;
	Tire.Longitudinal.PeakSlip = 0.1;
	Tire.Longitudinal.Shape = 1.65;
	Tire.Longitudinal.Curvature = 0.5;
	Tire.Lateral.PeakFriction = 1.05;
	Tire.Lateral.LoadSensitivity = -0.15;
	Tire.Lateral.PeakSlip = 0.12;
	Tire.Lateral.PeakSlipLoadSensitivity = 0.25;
	Tire.Lateral.Shape = 1.3;
	Tire.Lateral.Curvature = -0.5;

	Front = Axle;
	Front.PositionX = 1.453;
	Front.MountHeight = 0.030;
	Front.Suspension.SpringRate = 45000.0;
	Front.Suspension.BumpDamping = 2500.0;
	Front.Suspension.ReboundDamping = 4500.0;
	Front.Suspension.AntiRollRate = 20000.0;
	Front.MaxSteerAngle = 0.5;
	Front.BrakeTorque = 2500.0;

	Rear = Axle;
	Rear.PositionX = -1.189;
	Rear.MountHeight = 0.023;
	Rear.Suspension.SpringRate = 50000.0;
	Rear.Suspension.BumpDamping = 2800.0;
	Rear.Suspension.ReboundDamping = 5000.0;
	Rear.Suspension.AntiRollRate = 10000.0;
	Rear.BrakeTorque = 1200.0; // under hard braking the fronts lock first, which keeps the car straight

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
	Params.Drive.MaxTorque = DriveTorque;
	Params.Drive.MaxPower = DrivePower;
	Params.HandbrakeTorque = HandbrakeTorque;
	return Params;
}
