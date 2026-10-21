#pragma once

#include "CoreMinimal.h"
#include "Game/GTOLapTimer.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/GTOVehicleTypes.h"
#include "GTOVehiclePawn.generated.h"

class UBoxComponent;
class UCameraComponent;
class UGTOVehicleDataAsset;
class UGTOVehicleSimComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * The player's car. An invisible box is the Chaos body the sim adapter drives. Everything visible (body, wheels,
 * camera) hangs off a display root that follows the interpolated sim snapshots and never writes back.
 */
UCLASS()
class GTO_API AGTOVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AGTOVehiclePawn();

	/**
	 * Meshes and vehicle file, read again on every reset. Without an asset the class defaults apply. If the vehicle file
	 * cannot be loaded when the car spawns, the error goes to the log and the car destroys itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UGTOVehicleDataAsset> VehicleData;

	/** Puts the car back on the start point, level and facing the start direction, at rest with a fresh vehicle file. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ResetToStart();

	/**
	 * Puts the car on the ground below Transform, level and facing its yaw, at rest with a fresh vehicle file. The lap in
	 * progress no longer counts. A vehicle file that fails to load keeps the previous one.
	 */
	void ResetTo(const FTransform& Transform);

	const UGTOVehicleSimComponent* GetSim() const { return Sim; }

	/** Drives the car with Input instead of the player's controls, e.g. in tests; unset hands control back. */
	void SetScriptedInput(const TOptional<FGTODriverInput>& Input) { ScriptedInput = Input; }

	/** Driver input sent to the sim on the last frame. */
	const FGTODriverInput& GetLastInput() const { return LastInput; }

	/** Session lap times, or null on a map without a start-finish line. */
	const FGTOLapTimer* GetLapTimer() const { return LapTimer.GetPtrOrNull(); }

	/** Level of the traction control the player has chosen, 0 for off. */
	int32 GetTractionControl() const { return TractionControl; }

	/** The level the traction control key switches on. */
	static constexpr int32 DefaultTractionControl = 2;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	const UGTOVehicleDataAsset& GetVehicleData() const;
	void ApplyShapeAndMeshes(const UGTOVehicleDataAsset& Data);
	void PlaceMeshes(const UGTOVehicleDataAsset& Data);
	void UpdateDisplay();
	FGTODriverInput ReadDriverInput(float DeltaSeconds);
	void ToggleTractionControl();
	void ToggleTelemetry();
	void Quit();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Chassis;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> DisplayRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GlassMesh;

	/** In EGTOWheel order. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelMeshes;

	/** Placed and tilted from the vehicle data; its Z axis points along the column towards the driver. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SteeringColumn;

	/** Turns about the column. Carries a stand-in from basic shapes until the body mesh brings a steering wheel of its own. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SteeringWheel;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UGTOVehicleSimComponent> Sim;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HandbrakeAction;

	int32 TractionControl = DefaultTractionControl;

	/** The automatic is in reverse: the brake pedal drives backwards, the throttle pedal brakes. */
	bool bReverse = false;

	/** How long the car has stood with the brake held and no throttle (s). */
	float StandingOnBrake = 0.f;

	FTransform StartTransform;
	TOptional<FGTOLapTimer> LapTimer;
	TOptional<FGTODriverInput> ScriptedInput;
	FGTODriverInput LastInput;
};
