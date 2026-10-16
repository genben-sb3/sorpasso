#include "Vehicle/GTOVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/GTOHUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/GTOUnits.h"
#include "Vehicle/GTOVehicleDataAsset.h"
#include "Vehicle/GTOVehicleSimComponent.h"

using namespace GTOUnits;

namespace
{
	// The wheels hang this far above the ground after a reset and settle onto their springs.
	constexpr double ResetDropHeight = 0.02; // m

	UInputAction* MakeButtonAction(UObject* Outer, FName Name)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = EInputActionValueType::Boolean;
		return Action;
	}
}

AGTOVehiclePawn::AGTOVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // ResetToStart places the car

	static ConstructorHelpers::FObjectFinder<UGTOVehicleDataAsset> DataAsset(TEXT("/Game/Vehicles/SportsCar/DA_SportsCar.DA_SportsCar"));
	VehicleData = DataAsset.Object;

	// Centred on the centre of mass and clear of the ground at rest; only touches anything in a crash.
	// Mass and inertia come from the vehicle parameters, not from this shape.
	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	Chassis->SetBoxExtent(FVector(220.0, 85.0, 25.0));
	Chassis->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Chassis->SetSimulatePhysics(true);
	Chassis->SetLinearDamping(0.f);
	Chassis->SetAngularDamping(0.f);
	// Small force changes would otherwise put the body to sleep, and async forces do not wake it (ADR 0001).
	Chassis->BodyInstance.SleepFamily = ESleepFamily::Custom;
	Chassis->BodyInstance.CustomSleepThresholdMultiplier = 0.f;
	RootComponent = Chassis;

	DisplayRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DisplayRoot"));
	DisplayRoot->SetupAttachment(Chassis);
	DisplayRoot->SetUsingAbsoluteLocation(true);
	DisplayRoot->SetUsingAbsoluteRotation(true);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(DisplayRoot);
	BodyMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	GlassMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlassMesh"));
	GlassMesh->SetupAttachment(BodyMesh);
	GlassMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	const TCHAR* WheelNames[GTONumWheels] = { TEXT("WheelFrontLeft"), TEXT("WheelFrontRight"), TEXT("WheelRearLeft"), TEXT("WheelRearRight") };
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(WheelNames[Index]);
		Wheel->SetupAttachment(DisplayRoot);
		Wheel->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		// The wheel mesh is modelled for the left side, see UGTOVehicleDataAsset::WheelMesh.
		Wheel->SetRelativeRotation(FRotator(0.0, GTOIsLeft(static_cast<EGTOWheel>(Index)) ? 0.0 : 180.0, 0.0));
		WheelMeshes.Add(Wheel);
	}

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(DisplayRoot);
	CameraArm->SetRelativeLocation(FVector(0.0, 0.0, 80.0));
	CameraArm->SetRelativeRotation(FRotator(-10.0, 0.0, 0.0));
	CameraArm->TargetArmLength = 650.f;
	CameraArm->bDoCollisionTest = false;
	CameraArm->bInheritPitch = false;
	CameraArm->bInheritRoll = false;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 10.f;
	CameraArm->bEnableCameraRotationLag = true;
	CameraArm->CameraRotationLagSpeed = 8.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);

	Sim = CreateDefaultSubobject<UGTOVehicleSimComponent>(TEXT("Sim"));
}

void AGTOVehiclePawn::BeginPlay()
{
	Super::BeginPlay();
	AddTickPrerequisiteComponent(Sim); // display after the sim has advanced its interpolation
	StartTransform = GetActorTransform();
	ResetToStart();
}

void AGTOVehiclePawn::ResetToStart()
{
	const UGTOVehicleDataAsset* Data = VehicleData ? VehicleData.Get() : GetDefault<UGTOVehicleDataAsset>();
	const FGTOVehicleParams Params = Data->ToParams();
	ApplyMeshes(*Data);

	const FRotator Rotation(0.0, StartTransform.Rotator().Yaw, 0.0);
	FVector Location = StartTransform.GetLocation();
	FHitResult Hit;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(GTOResetGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, 200.0), Location - FVector(0.0, 0.0, 10000.0), ECC_Visibility, Query))
	{
		Location.Z = Hit.ImpactPoint.Z + (Params.FullDroopHeight() + ResetDropHeight) * CmPerM;
	}

	Chassis->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::ResetPhysics);
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Chassis->SetMassOverrideInKg(NAME_None, Params.Mass, true);
	Sim->Restart(Chassis, Params);
	UpdateDisplay();
}

void AGTOVehiclePawn::ApplyMeshes(const UGTOVehicleDataAsset& Data)
{
	BodyMesh->SetStaticMesh(Data.BodyMesh);
	BodyMesh->SetRelativeLocation(Data.BodyMeshOffset);
	GlassMesh->SetStaticMesh(Data.GlassMesh);
	for (UStaticMeshComponent* Wheel : WheelMeshes)
	{
		Wheel->SetStaticMesh(Data.WheelMesh);
	}
}

void AGTOVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Sim->QueueInput(FGTOInputPacket()); // neutral controls until driving input exists
	UpdateDisplay();
}

void AGTOVehiclePawn::UpdateDisplay()
{
	const FGTOVehicleState Shown = Sim->GetDisplayState();
	const FGTOVehicleParams Params = Sim->GetParams();

	DisplayRoot->SetWorldLocationAndRotation(Shown.Chassis.Position * CmPerM, Shown.Chassis.Orientation);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FVector Centre = Params.WheelCentreLocal(static_cast<EGTOWheel>(Index), Shown.Wheels[Index].Compression) * CmPerM;

		UStaticMeshComponent* WheelMesh = WheelMeshes[Index];
		const FVector MeshCentre = WheelMesh->GetStaticMesh() ? WheelMesh->GetStaticMesh()->GetBounds().Origin : FVector::ZeroVector;
		WheelMesh->SetRelativeLocation(Centre - WheelMesh->GetRelativeRotation().RotateVector(MeshCentre));
	}
}

void AGTOVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Created in code for now; they move to assets once the full key layout for driving arrives.
	UInputAction* ResetAction = MakeButtonAction(this, TEXT("IA_Reset"));
	UInputAction* TelemetryAction = MakeButtonAction(this, TEXT("IA_ToggleTelemetry"));
	UInputAction* QuitAction = MakeButtonAction(this, TEXT("IA_Quit"));

	InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle"));
	InputMapping->MapKey(ResetAction, EKeys::R);
	InputMapping->MapKey(ResetAction, EKeys::Gamepad_FaceButton_Top);
	InputMapping->MapKey(TelemetryAction, EKeys::T);
	InputMapping->MapKey(TelemetryAction, EKeys::Gamepad_Special_Left);
	InputMapping->MapKey(QuitAction, EKeys::Escape);

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(ResetAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::ResetToStart);
	Input->BindAction(TelemetryAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::ToggleTelemetry);
	Input->BindAction(QuitAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::Quit);

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}
}

void AGTOVehiclePawn::ToggleTelemetry()
{
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (AGTOHUD* HUD = PlayerController->GetHUD<AGTOHUD>())
		{
			HUD->ToggleTelemetry();
		}
	}
}

void AGTOVehiclePawn::Quit()
{
	UKismetSystemLibrary::QuitGame(this, Cast<APlayerController>(GetController()), EQuitPreference::Quit, false);
}
