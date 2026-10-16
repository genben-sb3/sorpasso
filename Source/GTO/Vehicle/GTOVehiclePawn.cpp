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
#include "InputModifiers.h"
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

	UInputAction* MakeAxisAction(UObject* Outer, FName Name, EInputActionAccumulationBehavior Accumulation = EInputActionAccumulationBehavior::TakeHighestAbsoluteValue)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = EInputActionValueType::Axis1D;
		Action->AccumulationBehavior = Accumulation;
		return Action;
	}

	void MapNegated(UInputMappingContext* Mapping, const UInputAction* Action, const FKey& Key)
	{
		Mapping->MapKey(Action, Key).Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
	}
}

AGTOVehiclePawn::AGTOVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // ResetToStart places the car

	static ConstructorHelpers::FObjectFinder<UGTOVehicleDataAsset> DataAsset(TEXT("/Game/Vehicles/SportsCar/DA_SportsCar.DA_SportsCar"));
	VehicleData = DataAsset.Object;

	// Centred on the centre of mass. Its size comes from the vehicle data; mass and inertia do not depend on it.
	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	Chassis->SetBoxExtent(GetVehicleData().CollisionExtent);
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
	const UGTOVehicleDataAsset& Data = GetVehicleData();
	const FGTOVehicleParams Params = Data.ToParams();
	ApplyShapeAndMeshes(Data);

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
	WheelSpin = TStaticArray<double, GTONumWheels>(InPlace, 0.0);
	UpdateDisplay();
}

const UGTOVehicleDataAsset& AGTOVehiclePawn::GetVehicleData() const
{
	return VehicleData ? *VehicleData : *GetDefault<UGTOVehicleDataAsset>();
}

void AGTOVehiclePawn::ApplyShapeAndMeshes(const UGTOVehicleDataAsset& Data)
{
	if (!Chassis->GetUnscaledBoxExtent().Equals(Data.CollisionExtent))
	{
		Chassis->SetBoxExtent(Data.CollisionExtent); // may recreate the physics body, so only on a change
	}
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
	LastInput = ScriptedInput ? *ScriptedInput : ReadDriverInput();
	Sim->QueueInput(LastInput);

	const FGTOVehicleState Shown = Sim->GetDisplayState();
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		WheelSpin[Index] = FMath::Fmod(WheelSpin[Index] + Shown.Wheels[Index].AngularVelocity * DeltaSeconds, UE_DOUBLE_TWO_PI);
	}
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

		// Steered about the vertical, spun about the axle. The right wheels are the left mesh turned around, so for them
		// rolling forward is a positive pitch in mesh space.
		const bool bLeft = GTOIsLeft(static_cast<EGTOWheel>(Index));
		const FRotator Rotation(FMath::RadiansToDegrees(WheelSpin[Index]) * (bLeft ? -1.0 : 1.0), FMath::RadiansToDegrees(Shown.Wheels[Index].SteerAngle) + (bLeft ? 0.0 : 180.0), 0.0);

		UStaticMeshComponent* WheelMesh = WheelMeshes[Index];
		const FVector MeshCentre = WheelMesh->GetStaticMesh() ? WheelMesh->GetStaticMesh()->GetBounds().Origin : FVector::ZeroVector;
		WheelMesh->SetRelativeLocationAndRotation(Centre - Rotation.RotateVector(MeshCentre), Rotation);
	}
}

void AGTOVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Default layout, created in code so it stays readable and reviewable. Keyboard and gamepad are mapped side by side
	// and both work at any time; for now their values go straight into the input packet.
	UInputAction* ResetAction = MakeButtonAction(this, TEXT("IA_Reset"));
	UInputAction* TelemetryAction = MakeButtonAction(this, TEXT("IA_ToggleTelemetry"));
	UInputAction* QuitAction = MakeButtonAction(this, TEXT("IA_Quit"));
	SteerAction = MakeAxisAction(this, TEXT("IA_Steer"), EInputActionAccumulationBehavior::Cumulative); // left and right keys cancel out
	ThrottleAction = MakeAxisAction(this, TEXT("IA_Throttle"));
	BrakeAction = MakeAxisAction(this, TEXT("IA_Brake"));
	HandbrakeAction = MakeAxisAction(this, TEXT("IA_Handbrake"));

	InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle"));
	InputMapping->MapKey(SteerAction, EKeys::D);
	InputMapping->MapKey(SteerAction, EKeys::Right);
	MapNegated(InputMapping, SteerAction, EKeys::A);
	MapNegated(InputMapping, SteerAction, EKeys::Left);
	UInputModifierDeadZone* StickDeadZone = NewObject<UInputModifierDeadZone>(InputMapping);
	StickDeadZone->Type = EDeadZoneType::Axial;
	StickDeadZone->LowerThreshold = 0.1f; // a resting stick may drift a little
	InputMapping->MapKey(SteerAction, EKeys::Gamepad_LeftX).Modifiers.Add(StickDeadZone);
	InputMapping->MapKey(ThrottleAction, EKeys::W);
	InputMapping->MapKey(ThrottleAction, EKeys::Up);
	InputMapping->MapKey(ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
	InputMapping->MapKey(BrakeAction, EKeys::S);
	InputMapping->MapKey(BrakeAction, EKeys::Down);
	InputMapping->MapKey(BrakeAction, EKeys::Gamepad_LeftTriggerAxis);
	InputMapping->MapKey(HandbrakeAction, EKeys::SpaceBar);
	InputMapping->MapKey(HandbrakeAction, EKeys::Gamepad_FaceButton_Bottom);
	InputMapping->MapKey(ResetAction, EKeys::R);
	InputMapping->MapKey(ResetAction, EKeys::Gamepad_FaceButton_Top);
	InputMapping->MapKey(TelemetryAction, EKeys::T);
	InputMapping->MapKey(TelemetryAction, EKeys::Gamepad_Special_Left);
	InputMapping->MapKey(QuitAction, EKeys::Escape);
	InputMapping->MapKey(QuitAction, EKeys::Gamepad_Special_Right);

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(ResetAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::ResetToStart);
	Input->BindAction(TelemetryAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::ToggleTelemetry);
	Input->BindAction(QuitAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::Quit);
	for (const UInputAction* Action : { SteerAction.Get(), ThrottleAction.Get(), BrakeAction.Get(), HandbrakeAction.Get() })
	{
		Input->BindActionValue(Action);
	}

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}
}

FGTOInputPacket AGTOVehiclePawn::ReadDriverInput() const
{
	FGTOInputPacket Packet;
	const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input || !SteerAction)
	{
		return Packet; // not driven by a player
	}
	const auto Value = [Input](const UInputAction* Action) { return Input->GetBoundActionValue(Action).Get<float>(); };
	Packet.Steer = FMath::Clamp(Value(SteerAction), -1.f, 1.f);
	Packet.Throttle = FMath::Clamp(Value(ThrottleAction), 0.f, 1.f);
	Packet.Brake = FMath::Clamp(Value(BrakeAction), 0.f, 1.f);
	Packet.Handbrake = FMath::Clamp(Value(HandbrakeAction), 0.f, 1.f);
	return Packet;
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
