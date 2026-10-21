#include "Vehicle/GTOVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/GTOHUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Track/GTOStartFinishLine.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/GTOUnits.h"
#include "Vehicle/GTOVehicleDataAsset.h"
#include "Vehicle/GTOVehicleSimComponent.h"
#include "ultragravity/vehicle_file.h"

using namespace GTOUnits;

DEFINE_LOG_CATEGORY_STATIC(LogGTOVehicle, Log, All);

namespace
{
	// The wheels hang this far above the ground after a reset and settle onto their springs.
	constexpr double ResetDropHeight = 0.02; // m

	// The stand-in steering wheel: a rim with a cross spoke and a lower spoke, so its turn shows (cm, in the column's
	// axes: Z towards the driver, X up along the wheel when it is straight).
	constexpr double SteeringWheelDiameter = 36.0; // cm
	constexpr double SteeringWheelThickness = 3.0; // cm
	constexpr double SpokeThickness = 3.5;         // cm

	UStaticMeshComponent* MakeShape(AActor* Owner, FName Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Location, const FVector& SizeCm)
	{
		UStaticMeshComponent* Shape = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Shape->SetupAttachment(Parent);
		Shape->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Shape->SetStaticMesh(Mesh);
		Shape->SetRelativeLocation(Location);
		Shape->SetRelativeScale3D(SizeCm / 100.0); // the basic shapes measure 1 m
		return Shape;
	}

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	SteeringColumn = CreateDefaultSubobject<USceneComponent>(TEXT("SteeringColumn"));
	SteeringColumn->SetupAttachment(DisplayRoot);
	SteeringWheel = CreateDefaultSubobject<USceneComponent>(TEXT("SteeringWheel"));
	SteeringWheel->SetupAttachment(SteeringColumn);
	constexpr double Front = 0.5 * SteeringWheelThickness + 0.5 * SpokeThickness; // cm, spokes on the driver's side of the rim
	MakeShape(this, TEXT("SteeringWheelRim"), SteeringWheel, Cylinder.Object, FVector::ZeroVector,
		FVector(SteeringWheelDiameter, SteeringWheelDiameter, SteeringWheelThickness));
	MakeShape(this, TEXT("SteeringWheelCrossSpoke"), SteeringWheel, Cube.Object, FVector(0.0, 0.0, Front),
		FVector(SpokeThickness, SteeringWheelDiameter, SpokeThickness));
	MakeShape(this, TEXT("SteeringWheelLowerSpoke"), SteeringWheel, Cube.Object, FVector(-0.25 * SteeringWheelDiameter, 0.0, Front),
		FVector(0.5 * SteeringWheelDiameter, SpokeThickness, SpokeThickness));

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
	if (const TActorIterator<AGTOStartFinishLine> Line(GetWorld()); Line)
	{
		LapTimer.Emplace(Line->GetTimingGate());
	}
	ResetToStart();
}

void AGTOVehiclePawn::ResetToStart()
{
	ResetTo(StartTransform);
}

void AGTOVehiclePawn::ResetTo(const FTransform& Transform)
{
	const UGTOVehicleDataAsset& Data = GetVehicleData();
	ultragravity::VehicleParams Params;
	FString Error;
	if (!Data.LoadVehicleParams(Params, Error))
	{
		UE_LOG(LogGTOVehicle, Error, TEXT("%s"), *Error);
		if (!Sim->HasParams())
		{
			UE_LOG(LogGTOVehicle, Error, TEXT("%s is not spawned without a vehicle file"), *GetName());
			Destroy();
		}
		return;
	}
	ApplyShapeAndMeshes(Data);

	const FRotator Rotation(0.0, Transform.Rotator().Yaw, 0.0);
	FVector Location = Transform.GetLocation();
	FHitResult Hit;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(GTOResetGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, 200.0), Location - FVector(0.0, 0.0, 10000.0), ECC_Visibility, Query))
	{
		Location.Z = Hit.ImpactPoint.Z + (UGTOVehicleSimComponent::FullDroopHeight(Params) + ResetDropHeight) * CmPerM;
	}

	Chassis->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::ResetPhysics);
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Chassis->SetMassOverrideInKg(NAME_None, Params.mass, true);
	Sim->Restart(Chassis, Params);
	PlaceMeshes(Data);
	BrakeToReverse.Reset();
	if (LapTimer)
	{
		LapTimer->Discard();
	}
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
	GlassMesh->SetStaticMesh(Data.GlassMesh);
	for (UStaticMeshComponent* Wheel : WheelMeshes)
	{
		Wheel->SetStaticMesh(Data.WheelMesh);
	}
}

void AGTOVehiclePawn::PlaceMeshes(const UGTOVehicleDataAsset& Data)
{
	BodyMesh->SetRelativeLocation(Sim->GetVehicleFrameOrigin() * CmPerM + Data.BodyMeshOrigin);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		WheelMeshes[Index]->SetRelativeScale3D(FVector(Sim->GetWheelRadius(static_cast<EGTOWheel>(Index)) * CmPerM / Data.WheelMeshRadius));
	}
	// Pitched so the column's Z axis leans from straight up back towards the driver by the tilt.
	SteeringColumn->SetRelativeLocationAndRotation(Sim->GetVehicleFrameOrigin() * CmPerM + Data.SteeringWheelOrigin, FRotator(90.0 - Data.SteeringWheelTilt, 0.0, 0.0));
}

void AGTOVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LastInput = ScriptedInput ? *ScriptedInput : BrakeToReverse.Apply(ReadDriverInput(), Sim->GetLatestState().Chassis.GetForwardSpeed(), DeltaSeconds);
	Sim->QueueInput(LastInput);

	if (LapTimer)
	{
		double SimTime = 0.0;
		const FGTOVehicleState Latest = Sim->GetLatestState(&SimTime);
		LapTimer->Update(SimTime, Latest.Chassis.Position);
	}

	UpdateDisplay();
}

void AGTOVehiclePawn::UpdateDisplay()
{
	const FGTOVehicleState Shown = Sim->GetDisplayState();

	DisplayRoot->SetWorldLocationAndRotation(Shown.Chassis.Position * CmPerM, Shown.Chassis.Orientation);
	for (int32 Index = 0; Index < GTONumWheels; ++Index)
	{
		const FVector Centre = Sim->GetWheelCentre(static_cast<EGTOWheel>(Index), Shown.Wheels[Index].Compression) * CmPerM;

		// Steered about the vertical, spun about the axle. The right wheels are the left mesh turned around, so for them
		// rolling forward is a positive pitch in mesh space.
		const bool bLeft = GTOIsLeft(static_cast<EGTOWheel>(Index));
		const FRotator Rotation(FMath::RadiansToDegrees(Shown.Wheels[Index].SpinAngle) * (bLeft ? -1.0 : 1.0), FMath::RadiansToDegrees(Shown.Wheels[Index].SteerAngle) + (bLeft ? 0.0 : 180.0), 0.0);

		UStaticMeshComponent* WheelMesh = WheelMeshes[Index];
		const FVector MeshCentre = WheelMesh->GetStaticMesh() ? WheelMesh->GetStaticMesh()->GetBounds().Origin * WheelMesh->GetRelativeScale3D() : FVector::ZeroVector;
		WheelMesh->SetRelativeLocationAndRotation(Centre - Rotation.RotateVector(MeshCentre), Rotation);
	}

	// A positive yaw about the column turns the wheel clockwise as the driver sees it, as for steering right.
	SteeringWheel->SetRelativeRotation(FRotator(0.0, FMath::RadiansToDegrees(Shown.SteeringWheelAngle), 0.0));
}

void AGTOVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Default layout, created in code so it stays readable and reviewable. Keyboard and gamepad are mapped side by side
	// and both work at any time; the adapter smooths their values (input smoothing of the physics package).
	UInputAction* ResetAction = MakeButtonAction(this, TEXT("IA_Reset"));
	UInputAction* TractionControlAction = MakeButtonAction(this, TEXT("IA_ToggleTractionControl"));
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
	InputMapping->MapKey(TractionControlAction, EKeys::C);
	InputMapping->MapKey(TractionControlAction, EKeys::Gamepad_DPad_Up);
	InputMapping->MapKey(TelemetryAction, EKeys::T);
	InputMapping->MapKey(TelemetryAction, EKeys::Gamepad_Special_Left);
	InputMapping->MapKey(QuitAction, EKeys::Escape);
	InputMapping->MapKey(QuitAction, EKeys::Gamepad_Special_Right);

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(ResetAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::ResetToStart);
	Input->BindAction(TractionControlAction, ETriggerEvent::Started, this, &AGTOVehiclePawn::ToggleTractionControl);
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

FGTODriverInput AGTOVehiclePawn::ReadDriverInput() const
{
	FGTODriverInput Driver;
	const UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input || !SteerAction)
	{
		return Driver; // not driven by a player
	}
	const auto Value = [Input](const UInputAction* Action) { return Input->GetBoundActionValue(Action).Get<float>(); };
	Driver.Steer = FMath::Clamp(Value(SteerAction), -1.f, 1.f);
	Driver.Throttle = FMath::Clamp(Value(ThrottleAction), 0.f, 1.f);
	Driver.Brake = FMath::Clamp(Value(BrakeAction), 0.f, 1.f);
	Driver.Handbrake = FMath::Clamp(Value(HandbrakeAction), 0.f, 1.f);
	Driver.TractionControl = TractionControl;
	Driver.bHuman = true;
	return Driver;
}

void AGTOVehiclePawn::ToggleTractionControl()
{
	TractionControl = TractionControl > 0 ? 0 : DefaultTractionControl;
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
