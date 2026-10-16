#pragma once

#include "CoreMinimal.h"

/** One piece of a track's centreline: a straight, or an arc of constant radius. */
struct FGTOTrackSegment
{
	double Length = 0.0; // m along the centreline
	double Turn = 0.0;   // rad, heading change over the segment, positive to the right; 0 for a straight

	static FGTOTrackSegment Straight(double Length);
	static FGTOTrackSegment Corner(double Radius, double TurnDegrees);

	bool IsStraight() const { return Turn == 0.0; }
	double Radius() const { return IsStraight() ? TNumericLimits<double>::Max() : Length / FMath::Abs(Turn); }
};

/** A point on the centreline on flat ground (z = 0), in metres. */
struct FGTOTrackPose
{
	FVector2D Location = FVector2D::ZeroVector;
	double Heading = 0.0; // rad, yaw from +X towards +Y (right), counted on over the whole lap

	FVector2D Forward() const { return FVector2D(FMath::Cos(Heading), FMath::Sin(Heading)); }
	FVector2D Right() const { return FVector2D(-FMath::Sin(Heading), FMath::Cos(Heading)); }
};

/**
 * Shape across the track, the same on both sides, by distance from the centreline: flat road, then a verge that is
 * a curb where there is one and otherwise slopes down to the grass, then an edge strip that runs on down below the
 * grass, so the grass ground hides where it ends. All in metres.
 */
struct GTO_API FGTOTrackCrossSection
{
	double HalfWidth = 6.0;   // road
	double CurbWidth = 1.2;   // verge beyond the road edge, curb or grass
	double CurbRise = 0.3;    // width over which the curb climbs to its full height
	double CurbHeight = 0.03;
	double GrassDrop = 0.015; // grass level below the road
	double EdgeWidth = 0.5;
	double EdgeTuck = 0.02;   // how far the edge strip ends below the grass

	double VergeOuterOffset() const { return HalfWidth + CurbWidth; }

	/**
	 * Height of the surface a wheel rolls on at Lateral metres from the centreline (either side), where the curb has
	 * the given weight, 0 to 1. Smooth across the whole width: without curb the verge slopes down to the grass, with
	 * curb it rises and holds.
	 */
	double HeightAt(double Lateral, double CurbWeight) const;

	/** Verge height Offset metres beyond the road edge, 0 to CurbWidth. */
	double VergeHeightAt(double Offset, double CurbWeight) const;

	/** Edge strip height Offset metres beyond the verge, 0 to EdgeWidth; ends EdgeTuck below the grass. */
	double EdgeHeightAt(double Offset, double CurbWeight) const;
};

/**
 * Closed, flat circuit: the centreline from straights and arcs, starting at the origin heading +X, plus the curbs and
 * the start-finish line. Two of the straights are closing straights, their lengths solved so that the loop closes.
 */
class GTO_API FGTOTrackLayout
{
public:
	/** Curbs run on this far past both ends of a corner, then ramp down over CurbRamp. */
	static constexpr double CurbOverrun = 5.0; // m
	static constexpr double CurbRamp = 4.0;    // m

	/** Empty; see TestTrack(). */
	FGTOTrackLayout() = default;

	/** ClosingA and ClosingB are indices of two non-parallel straights; their given lengths are replaced. */
	FGTOTrackLayout(TArray<FGTOTrackSegment> InSegments, int32 ClosingA, int32 ClosingB, const FGTOTrackCrossSection& InCrossSection, double InStartLineDistance);

	/** The M1 test track: about 2.4 km with long straights, fast and slow corners, a chicane and a hairpin. */
	static FGTOTrackLayout TestTrack();

	double GetLength() const { return Length; }
	const TArray<FGTOTrackSegment>& GetSegments() const { return Segments; }
	double GetSegmentStart(int32 Index) const { return Starts[Index].Distance; }
	const FGTOTrackCrossSection& GetCrossSection() const { return CrossSection; }

	/** Distance of the start-finish line along the centreline. */
	double GetStartLineDistance() const { return StartLineDistance; }

	/** Centreline at Distance metres from the origin, wrapped onto the lap. */
	FGTOTrackPose PoseAt(double Distance) const;

	/** How much curb there is at Distance, 0 to 1, on both sides of the road. */
	double CurbWeightAt(double Distance) const;

private:
	struct FSegmentStart
	{
		double Distance = 0.0;
		FGTOTrackPose Pose;
	};

	void Chain();

	TArray<FGTOTrackSegment> Segments;
	TArray<FSegmentStart> Starts;
	FGTOTrackCrossSection CrossSection;
	double Length = 0.0;
	double StartLineDistance = 0.0;
};
