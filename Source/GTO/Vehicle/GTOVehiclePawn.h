#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
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

	/** Vehicle parameters and meshes, read again on every reset. Without an asset the class defaults apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	TObjectPtr<UGTOVehicleDataAsset> VehicleData;

	/** Puts the car back on the start point, level and facing the start direction, at rest with fresh parameters. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ResetToStart();

	const UGTOVehicleSimComponent* GetSim() const { return Sim; }

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void ApplyMeshes(const UGTOVehicleDataAsset& Data);
	void UpdateDisplay();
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

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UGTOVehicleSimComponent> Sim;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputMapping;

	FTransform StartTransform;
};
