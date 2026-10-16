#include "Track/GTOTestTrack.h"

#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Vehicle/GTOUnits.h"

using namespace GTOUnits;

namespace
{
	constexpr double SampleSpacing = 1.0;   // m between cross-sections along the track
	constexpr double GroundCellSize = 50.0; // m

	/** One mesh section, built quad by quad. Positions in metres, stored in centimetres. */
	struct FMeshBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;

		int32 AddVertex(const FVector& Position, const FVector& Normal, const FVector2D& UV, const FVector& Tangent)
		{
			Vertices.Add(Position * CmPerM);
			Normals.Add(Normal);
			UVs.Add(UV);
			Tangents.Emplace(Tangent, false);
			return Vertices.Num() - 1;
		}

		/** A quad facing up: Left0 and Right0 on one row, Left1 and Right1 on the next one ahead, Right to the right. */
		void AddQuad(int32 Left0, int32 Right0, int32 Left1, int32 Right1)
		{
			Triangles.Append({ Left0, Right0, Left1, Left1, Right0, Right1 });
		}

		void Upload(UProceduralMeshComponent& Mesh, UMaterialInterface* Material) const
		{
			Mesh.ClearAllMeshSections();
			Mesh.CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);
			Mesh.SetMaterial(0, Material);
		}
	};

	/**
	 * Height of the mesh surface Lateral metres from the centreline. Like FGTOTrackCrossSection::HeightAt, except that
	 * the edge strip runs on below the grass instead of stopping at it.
	 */
	double MeshHeightAt(const FGTOTrackCrossSection& Section, double Lateral, double CurbWeight)
	{
		const double Offset = FMath::Abs(Lateral) - Section.HalfWidth;
		return Offset <= Section.CurbWidth ? Section.HeightAt(Lateral, CurbWeight) : Section.EdgeHeightAt(Offset - Section.CurbWidth, CurbWeight);
	}

	UProceduralMeshComponent* CreateSurface(AActor& Owner, USceneComponent* Parent, FName Name)
	{
		UProceduralMeshComponent* Mesh = Owner.CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		Mesh->SetupAttachment(Parent);
		Mesh->bUseAsyncCooking = false; // the car's wheels need the ground on the first physics step
		Mesh->bUseComplexAsSimpleCollision = true;
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return Mesh;
	}
}

AGTOTestTrack::AGTOTestTrack()
	: Layout(FGTOTrackLayout::TestTrack())
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RoadMesh = CreateSurface(*this, RootComponent, TEXT("Road"));
	CurbMesh = CreateSurface(*this, RootComponent, TEXT("Curbs"));
	GrassMesh = CreateSurface(*this, RootComponent, TEXT("Grass"));
}

void AGTOTestTrack::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildMeshes();
}

void AGTOTestTrack::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Levels loaded for play do not run the construction again; build from the current layout before anything drives.
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		BuildMeshes();
	}
}

FTransform AGTOTestTrack::GetTrackTransform(double Distance, double Lateral) const
{
	const FGTOTrackPose Pose = Layout.PoseAt(Distance);
	const FVector2D Location = Pose.Location + Pose.Right() * Lateral;
	const double Height = Layout.GetCrossSection().HeightAt(Lateral, Layout.CurbWeightAt(Distance));
	const FTransform Local(FRotator(0.0, FMath::RadiansToDegrees(Pose.Heading), 0.0), FVector(Location, Height) * CmPerM);
	return Local * GetActorTransform();
}

FTransform AGTOTestTrack::GetStartLineTransform() const
{
	return GetTrackTransform(Layout.GetStartLineDistance(), 0.0);
}

FTransform AGTOTestTrack::GetGridTransform() const
{
	return GetTrackTransform(Layout.GetStartLineDistance() - GridSetback, 0.0);
}

void AGTOTestTrack::BuildMeshes()
{
	const FGTOTrackCrossSection& Section = Layout.GetCrossSection();
	const int32 NumRows = FMath::CeilToInt32(Layout.GetLength() / SampleSpacing);
	const double Spacing = Layout.GetLength() / NumRows;

	const auto Point = [&](double Distance, double Lateral)
	{
		const FGTOTrackPose Pose = Layout.PoseAt(Distance);
		return FVector(Pose.Location + Pose.Right() * Lateral, MeshHeightAt(Section, Lateral, Layout.CurbWeightAt(Distance)));
	};

	// Normals and tangents from the surface itself, so shading stays smooth across quads that share no vertices.
	const auto AddSurfaceVertex = [&](FMeshBuffers& Buffers, double Distance, double Lateral)
	{
		constexpr double Delta = 0.01;
		const FVector Along = Point(Distance + Delta, Lateral) - Point(Distance - Delta, Lateral);
		const FVector Across = Point(Distance, Lateral + Delta) - Point(Distance, Lateral - Delta);
		const FVector Normal = FVector::CrossProduct(Along, Across).GetSafeNormal();
		return Buffers.AddVertex(Point(Distance, Lateral), Normal, FVector2D(Distance, Lateral), Along.GetSafeNormal());
	};

	const auto AddBand = [&](FMeshBuffers& Buffers, double Distance0, double Distance1, double Left, double Right)
	{
		Buffers.AddQuad(AddSurfaceVertex(Buffers, Distance0, Left), AddSurfaceVertex(Buffers, Distance0, Right),
			AddSurfaceVertex(Buffers, Distance1, Left), AddSurfaceVertex(Buffers, Distance1, Right));
	};

	// Offsets beyond the road edge where the cross-section bends: through the curb's rise, across the verge, and the edge.
	TArray<double> Offsets;
	for (int32 Step = 0; Step <= 3; ++Step)
	{
		Offsets.Add(Section.CurbRise * Step / 3.0);
	}
	Offsets.Append({ 0.5 * (Section.CurbRise + Section.CurbWidth), Section.CurbWidth, Section.CurbWidth + Section.EdgeWidth });

	FMeshBuffers Road, Curbs, Grass;
	FBox2D Bounds(ForceInit);
	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		const double Distance0 = Row * Spacing;
		const double Distance1 = (Row + 1) * Spacing;
		Bounds += Layout.PoseAt(Distance0).Location;

		AddBand(Road, Distance0, Distance1, -Section.HalfWidth, Section.HalfWidth);

		const bool bCurb = Layout.CurbWeightAt(Distance0) > 0.0 || Layout.CurbWeightAt(Distance1) > 0.0;
		FMeshBuffers& Verge = bCurb ? Curbs : Grass;
		for (int32 Index = 0; Index + 1 < Offsets.Num(); ++Index)
		{
			const double Inner = Section.HalfWidth + Offsets[Index];
			const double Outer = Section.HalfWidth + Offsets[Index + 1];
			AddBand(Verge, Distance0, Distance1, Inner, Outer);   // right side
			AddBand(Verge, Distance0, Distance1, -Outer, -Inner); // left side
		}
	}

	// Grass ground, a grid of flat cells at grass level.
	const int32 NumCells = FMath::CeilToInt32(GroundSize / GroundCellSize);
	const double CellSize = GroundSize / NumCells;
	const FVector2D Corner = Bounds.GetCenter() - FVector2D(0.5 * GroundSize);
	const int32 FirstGroundVertex = Grass.Vertices.Num();
	for (int32 Y = 0; Y <= NumCells; ++Y)
	{
		for (int32 X = 0; X <= NumCells; ++X)
		{
			const FVector2D Location = Corner + FVector2D(X, Y) * CellSize;
			Grass.AddVertex(FVector(Location, -Section.GrassDrop), FVector::UpVector, Location, FVector::ForwardVector);
		}
	}
	for (int32 Y = 0; Y < NumCells; ++Y)
	{
		for (int32 X = 0; X < NumCells; ++X)
		{
			// Rows run along +X, so "right" is +Y.
			const int32 Here = FirstGroundVertex + Y * (NumCells + 1) + X;
			Grass.AddQuad(Here, Here + NumCells + 1, Here + 1, Here + NumCells + 2);
		}
	}

	Road.Upload(*RoadMesh, RoadMaterial);
	Curbs.Upload(*CurbMesh, CurbMaterial);
	Grass.Upload(*GrassMesh, GrassMaterial);
}
