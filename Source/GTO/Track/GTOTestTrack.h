#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Track/GTOTrackLayout.h"
#include "GTOTestTrack.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * The M1 test track, built from FGTOTrackLayout::TestTrack(): road, curbs, grass verges and a square of grass ground
 * around it, flat. Each surface is its own mesh with collision, and its grip is the friction of the physical material
 * set on its material (complex collision takes the physical material from the material, not from an override).
 */
UCLASS()
class GTO_API AGTOTestTrack : public AActor
{
	GENERATED_BODY()

public:
	AGTOTestTrack();

	UPROPERTY(EditAnywhere, Category = "Track")
	TObjectPtr<UMaterialInterface> RoadMaterial;

	UPROPERTY(EditAnywhere, Category = "Track")
	TObjectPtr<UMaterialInterface> CurbMaterial;

	UPROPERTY(EditAnywhere, Category = "Track")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	/** Side of the square grass ground, centred on the track (m). */
	UPROPERTY(EditAnywhere, Category = "Track", meta = (ClampMin = 100))
	double GroundSize = 3000.0;

	/** The car starts this far behind the start-finish line (m, to the centre of mass). */
	UPROPERTY(EditAnywhere, Category = "Track", meta = (ClampMin = 0))
	double GridSetback = 5.0;

	/**
	 * World transform of the point Distance metres along the centreline and Lateral metres to its right, on the
	 * surface, X along the driving direction.
	 */
	UFUNCTION(BlueprintCallable, Category = "Track")
	FTransform GetTrackTransform(double Distance, double Lateral) const;

	/** On the start-finish line, in the middle of the road. */
	UFUNCTION(BlueprintCallable, Category = "Track")
	FTransform GetStartLineTransform() const;

	/** Where the car starts: GridSetback behind the start-finish line. */
	UFUNCTION(BlueprintCallable, Category = "Track")
	FTransform GetGridTransform() const;

	/** Road width (m). */
	UFUNCTION(BlueprintCallable, Category = "Track")
	double GetRoadWidth() const { return 2.0 * Layout.GetCrossSection().HalfWidth; }

	const FGTOTrackLayout& GetLayout() const { return Layout; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;

private:
	void BuildMeshes();

	FGTOTrackLayout Layout;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> RoadMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> CurbMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> GrassMesh;
};
