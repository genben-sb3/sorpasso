#include "Track/GTOTrackLayout.h"

#include "Algo/BinarySearch.h"

namespace
{
	/** Pose Distance metres into Segment from Start: exact on arcs. */
	FGTOTrackPose Advance(const FGTOTrackPose& Start, const FGTOTrackSegment& Segment, double Distance)
	{
		FGTOTrackPose Pose;
		if (Segment.IsStraight())
		{
			Pose.Location = Start.Location + Start.Forward() * Distance;
			Pose.Heading = Start.Heading;
			return Pose;
		}
		const double Curvature = Segment.Turn / Segment.Length; // rad/m, positive to the right
		Pose.Heading = Start.Heading + Curvature * Distance;
		Pose.Location = Start.Location + FVector2D(
			FMath::Sin(Pose.Heading) - FMath::Sin(Start.Heading),
			FMath::Cos(Start.Heading) - FMath::Cos(Pose.Heading)) / Curvature;
		return Pose;
	}
}

FGTOTrackSegment FGTOTrackSegment::Straight(double Length)
{
	return FGTOTrackSegment{ Length, 0.0 };
}

FGTOTrackSegment FGTOTrackSegment::Corner(double Radius, double TurnDegrees)
{
	const double Turn = FMath::DegreesToRadians(TurnDegrees);
	return FGTOTrackSegment{ Radius * FMath::Abs(Turn), Turn };
}

double FGTOTrackCrossSection::VergeHeightAt(double Offset, double CurbWeight) const
{
	const double Curb = CurbHeight * FMath::SmoothStep(0.0, CurbRise, Offset);
	const double Slope = -GrassDrop * Offset / CurbWidth;
	return FMath::Lerp(Slope, Curb, CurbWeight);
}

double FGTOTrackCrossSection::EdgeHeightAt(double Offset, double CurbWeight) const
{
	const double Top = VergeHeightAt(CurbWidth, CurbWeight);
	return FMath::Lerp(Top, -GrassDrop - EdgeTuck, FMath::Clamp(Offset / EdgeWidth, 0.0, 1.0));
}

double FGTOTrackCrossSection::HeightAt(double Lateral, double CurbWeight) const
{
	const double Offset = FMath::Abs(Lateral) - HalfWidth; // beyond the road edge
	if (Offset <= 0.0)
	{
		return 0.0;
	}
	if (Offset <= CurbWidth)
	{
		return VergeHeightAt(Offset, CurbWeight);
	}
	return FMath::Max(EdgeHeightAt(Offset - CurbWidth, CurbWeight), -GrassDrop);
}

FGTOTrackLayout::FGTOTrackLayout(TArray<FGTOTrackSegment> InSegments, int32 ClosingA, int32 ClosingB, const FGTOTrackCrossSection& InCrossSection, double InStartLineDistance)
	: Segments(MoveTemp(InSegments))
	, CrossSection(InCrossSection)
	, StartLineDistance(InStartLineDistance)
{
	check(Segments.IsValidIndex(ClosingA) && Segments[ClosingA].IsStraight());
	check(Segments.IsValidIndex(ClosingB) && Segments[ClosingB].IsStraight());

	// The closing straights only shift everything after them along their own heading, so the end of the loop moves
	// linearly with their lengths: End + A·DirA + B·DirB = origin.
	Segments[ClosingA].Length = 0.0;
	Segments[ClosingB].Length = 0.0;
	Chain();
	const FVector2D End = Advance(Starts.Last().Pose, Segments.Last(), Segments.Last().Length).Location;
	const FVector2D DirA = Starts[ClosingA].Pose.Forward();
	const FVector2D DirB = Starts[ClosingB].Pose.Forward();
	const double Determinant = DirA ^ DirB;
	check(FMath::Abs(Determinant) > 1.0e-6); // the closing straights must not be parallel
	Segments[ClosingA].Length = (DirB ^ End) / Determinant;
	Segments[ClosingB].Length = (End ^ DirA) / Determinant;
	check(Segments[ClosingA].Length >= 0.0 && Segments[ClosingB].Length >= 0.0);
	Chain();
}

FGTOTrackLayout FGTOTrackLayout::TestTrack()
{
	using S = FGTOTrackSegment;
	return FGTOTrackLayout({
		S::Straight(450.0),                                                // 0  main straight, start-finish line
		S::Corner(180.0, 70.0),                                            // 1  fast right
		S::Straight(120.0),
		S::Corner(60.0, 110.0),                                            // 3  medium right
		S::Straight(90.0),
		S::Corner(20.0, -40.0), S::Corner(20.0, 80.0), S::Corner(20.0, -40.0), // 5–7  chicane
		S::Straight(90.0),
		S::Corner(15.0, -180.0),                                           // 9  hairpin
		S::Straight(120.0),
		S::Corner(30.0, 90.0),                                             // 11 slow right
		S::Straight(60.0),
		S::Corner(40.0, 90.0),                                             // 13 slow right onto the back straight
		S::Straight(0.0),                                                  // 14 back straight, closing
		S::Corner(60.0, 90.0),                                             // 15 medium right
		S::Straight(0.0),                                                  // 16 closing
		S::Corner(150.0, -35.0), S::Straight(40.0), S::Corner(150.0, 35.0), // 17–19 fast esses
		S::Straight(60.0),
		S::Corner(50.0, 90.0),                                             // 21 onto the main straight
	}, 14, 16, FGTOTrackCrossSection(), 150.0);
}

void FGTOTrackLayout::Chain()
{
	Starts.SetNum(Segments.Num());
	FGTOTrackPose Pose;
	double Distance = 0.0;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		Starts[Index].Distance = Distance;
		Starts[Index].Pose = Pose;
		Pose = Advance(Pose, Segments[Index], Segments[Index].Length);
		Distance += Segments[Index].Length;
	}
	Length = Distance;
}

FGTOTrackPose FGTOTrackLayout::PoseAt(double Distance) const
{
	double Wrapped = FMath::Fmod(Distance, Length);
	if (Wrapped < 0.0)
	{
		Wrapped += Length;
	}
	const int32 Index = FMath::Max(0, static_cast<int32>(Algo::UpperBoundBy(Starts, Wrapped, &FSegmentStart::Distance)) - 1);
	return Advance(Starts[Index].Pose, Segments[Index], Wrapped - Starts[Index].Distance);
}

double FGTOTrackLayout::CurbWeightAt(double Distance) const
{
	double Wrapped = FMath::Fmod(Distance, Length);
	if (Wrapped < 0.0)
	{
		Wrapped += Length;
	}

	double Weight = 0.0;
	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		if (Segments[Index].IsStraight())
		{
			continue;
		}
		const double First = Starts[Index].Distance - CurbOverrun;
		const double Last = Starts[Index].Distance + Segments[Index].Length + CurbOverrun;
		for (const double Lap : { -Length, 0.0, Length }) // corners near the origin reach across it
		{
			const double At = Wrapped + Lap;
			Weight = FMath::Max(Weight, FMath::SmoothStep(First - CurbRamp, First, At) * (1.0 - FMath::SmoothStep(Last, Last + CurbRamp, At)));
		}
	}
	return Weight;
}
