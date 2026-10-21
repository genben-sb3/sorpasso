#include "Vehicle/GTOVehicleDataAsset.h"

#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"
#include "ultragravity/vehicle_file.h"

UGTOVehicleDataAsset::UGTOVehicleDataAsset()
{
	// SportsCar mesh of the UE Vehicle Template: in mesh space the wheel centres sit 1.35 m ahead of and 1.29 m behind
	// the origin, 0.25 m above it, with a tyre radius of 0.393 m. BodyMeshOrigin puts the front wheel arches over the
	// front axle of the vehicle file and the arch centres at the height of its wheel centres at rest (0.33 m).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar.SM_SportsCar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GlassAsset(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Glass.SM_SportsCar_Glass"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelAsset(TEXT("/Game/Vehicles/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel"));
	BodyMesh = BodyAsset.Object;
	GlassMesh = GlassAsset.Object;
	WheelMesh = WheelAsset.Object;
}

FString UGTOVehicleDataAsset::GetVehicleFilePath() const
{
	return FPaths::ProjectContentDir() / VehicleFile;
}

bool UGTOVehicleDataAsset::LoadVehicleParams(ultragravity::VehicleParams& OutParams, FString& OutError) const
{
	const FString Path = GetVehicleFilePath();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("vehicle file %s: cannot be read"), *Path);
		return false;
	}

	// The physics package parses and checks the text; it opens no files inside Unreal (ADR 0010).
	ultragravity::LoadResult Result = ultragravity::parse_vehicle_file(std::string_view(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num()));
	if (Result.error)
	{
		OutError = FString::Printf(TEXT("vehicle file %s: %s: %s"), *Path, UTF8_TO_TCHAR(Result.error->field.c_str()), UTF8_TO_TCHAR(Result.error->message.c_str()));
		return false;
	}
	OutParams = MoveTemp(Result.params);
	return true;
}
