#include "Track/GTOStartFinishLine.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Vehicle/GTOUnits.h"

using namespace GTOUnits;

namespace
{
	constexpr double StripeLength = 0.5;  // m along the track
	constexpr double StripeLift = 0.005;  // m above the road, against z-fighting
	constexpr double GateHalfHeight = 3.0; // m
}

AGTOStartFinishLine::AGTOStartFinishLine()
{
	Gate = CreateDefaultSubobject<UBoxComponent>(TEXT("Gate"));
	Gate->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Gate->SetGenerateOverlapEvents(false);
	Gate->SetHiddenInGame(true);
	RootComponent = Gate;

	Stripe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stripe"));
	Stripe->SetupAttachment(Gate);
	Stripe->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Stripe->SetRelativeLocation(FVector(0.0, 0.0, StripeLift * CmPerM));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane")); // 1 m square, facing up
	Stripe->SetStaticMesh(Plane.Object);
	ApplySettings();
}

void AGTOStartFinishLine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySettings();
}

void AGTOStartFinishLine::ApplySettings()
{
	Gate->SetBoxExtent(FVector(0.5 * StripeLength, 0.5 * Width + GateMargin, GateHalfHeight) * CmPerM);
	Stripe->SetRelativeScale3D(FVector(StripeLength, Width, 1.0));
	Stripe->SetMaterial(0, StripeMaterial);
}

FGTOTimingGate AGTOStartFinishLine::GetTimingGate() const
{
	const FVector Extent = Gate->GetScaledBoxExtent() / CmPerM;
	FGTOTimingGate Result;
	Result.Centre = Gate->GetComponentLocation() / CmPerM;
	Result.Forward = Gate->GetForwardVector();
	Result.HalfWidth = Extent.Y;
	Result.HalfHeight = Extent.Z;
	return Result;
}
