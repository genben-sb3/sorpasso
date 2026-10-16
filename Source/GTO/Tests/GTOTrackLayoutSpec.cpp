#include "Misc/AutomationTest.h"
#include "Track/GTOTrackLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Indices of the corners (arcs) of a layout, in driving order. */
	TArray<int32> Corners(const FGTOTrackLayout& Layout)
	{
		TArray<int32> Result;
		for (int32 Index = 0; Index < Layout.GetSegments().Num(); ++Index)
		{
			if (!Layout.GetSegments()[Index].IsStraight())
			{
				Result.Add(Index);
			}
		}
		return Result;
	}

	double Mid(const FGTOTrackLayout& Layout, int32 Segment)
	{
		return Layout.GetSegmentStart(Segment) + 0.5 * Layout.GetSegments()[Segment].Length;
	}
}

BEGIN_DEFINE_SPEC(FGTOTrackLayoutSpec, "GTO.Track.Layout", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOTrackLayoutSpec)

void FGTOTrackLayoutSpec::Define()
{
	Describe("any layout", [this]()
	{
		It("closes the loop by solving the lengths of its two closing straights", [this]()
		{
			// A rectangle with one side too short and one too long: the closing straights make up the difference.
			TArray<FGTOTrackSegment> Segments = {
				FGTOTrackSegment::Straight(100.0), FGTOTrackSegment::Corner(20.0, 90.0),
				FGTOTrackSegment::Straight(10.0), FGTOTrackSegment::Corner(20.0, 90.0),
				FGTOTrackSegment::Straight(0.0), FGTOTrackSegment::Corner(20.0, 90.0),
				FGTOTrackSegment::Straight(0.0), FGTOTrackSegment::Corner(20.0, 90.0),
			};
			const FGTOTrackLayout Layout(Segments, 4, 6, FGTOTrackCrossSection(), 10.0);
			TestEqual(TEXT("closing straight along -X"), Layout.GetSegments()[4].Length, 100.0, 1.0e-6);
			TestEqual(TEXT("closing straight along -Y"), Layout.GetSegments()[6].Length, 10.0, 1.0e-6);
			TestEqual(TEXT("length"), Layout.GetLength(), 220.0 + UE_DOUBLE_TWO_PI * 20.0, 1.0e-6);
		});

		It("follows its arcs exactly", [this]()
		{
			const FGTOTrackLayout Layout({
				FGTOTrackSegment::Straight(50.0), FGTOTrackSegment::Corner(25.0, 180.0),
				FGTOTrackSegment::Straight(0.0), FGTOTrackSegment::Corner(25.0, 90.0),
				FGTOTrackSegment::Straight(0.0), FGTOTrackSegment::Corner(25.0, 90.0),
			}, 2, 4, FGTOTrackCrossSection(), 10.0);
			// Half way round the first corner, turning right from +X: 25 m on from its start, 25 m to the right (+Y).
			const FGTOTrackPose Pose = Layout.PoseAt(50.0 + 0.5 * UE_DOUBLE_PI * 25.0);
			TestEqual(TEXT("x"), Pose.Location.X, 75.0, 1.0e-6);
			TestEqual(TEXT("y"), Pose.Location.Y, 25.0, 1.0e-6);
			TestEqual(TEXT("heading"), Pose.Heading, UE_DOUBLE_HALF_PI, 1.0e-9);
		});
	});

	Describe("the test track", [this]()
	{
		It("is a closed loop without jumps", [this]()
		{
			const FGTOTrackLayout Layout = FGTOTrackLayout::TestTrack();
			const FGTOTrackPose Start = Layout.PoseAt(0.0);
			const FGTOTrackPose End = Layout.PoseAt(Layout.GetLength() - 1.0e-9);
			TestTrue(FString::Printf(TEXT("ends where it starts (%g m off)"), FVector2D::Distance(Start.Location, End.Location)), FVector2D::Distance(Start.Location, End.Location) < 0.01);
			TestEqual(TEXT("ends heading where it starts, one turn on"), End.Heading - Start.Heading, UE_DOUBLE_TWO_PI, 1.0e-6);

			constexpr double Step = 0.5;
			FGTOTrackPose Previous = Start;
			double WorstGap = 0.0;
			for (double Distance = Step; Distance < Layout.GetLength(); Distance += Step)
			{
				const FGTOTrackPose Pose = Layout.PoseAt(Distance);
				WorstGap = FMath::Max(WorstGap, FMath::Abs(FVector2D::Distance(Pose.Location, Previous.Location) - Step));
				Previous = Pose;
			}
			TestTrue(FString::Printf(TEXT("consecutive points 0.5 m apart (worst off by %g m)"), WorstGap), WorstGap < 0.01);
		});

		It("is shorter than the later 3 to 4 km circuit", [this]()
		{
			const double Length = FGTOTrackLayout::TestTrack().GetLength();
			TestTrue(FString::Printf(TEXT("length %.0f m between 1.5 and 3 km"), Length), Length > 1500.0 && Length < 3000.0);
		});

		It("never comes close to itself", [this]()
		{
			// Two points far apart along the track keep their verges apart with grass between them.
			const FGTOTrackLayout Layout = FGTOTrackLayout::TestTrack();
			const double MinGap = 2.0 * Layout.GetCrossSection().VergeOuterOffset() + 5.0;
			constexpr double Step = 2.0;
			double Closest = TNumericLimits<double>::Max();
			for (double A = 0.0; A < Layout.GetLength(); A += Step)
			{
				for (double B = A + 100.0; B < Layout.GetLength() + A - 100.0 && B < Layout.GetLength(); B += Step)
				{
					Closest = FMath::Min(Closest, FVector2D::Distance(Layout.PoseAt(A).Location, Layout.PoseAt(B).Location));
				}
			}
			TestTrue(FString::Printf(TEXT("closest approach %.1f m at least %.1f m"), Closest, MinGap), Closest >= MinGap);
		});

		It("has long straights, fast and slow corners, a chicane and a hairpin", [this]()
		{
			const FGTOTrackLayout Layout = FGTOTrackLayout::TestTrack();
			double LongestStraight = 0.0;
			bool bFastCorner = false, bSlowCorner = false, bHairpin = false, bChicane = false;
			const TArray<FGTOTrackSegment>& Segments = Layout.GetSegments();
			for (int32 Index = 0; Index < Segments.Num(); ++Index)
			{
				const FGTOTrackSegment& Segment = Segments[Index];
				const double Degrees = FMath::RadiansToDegrees(FMath::Abs(Segment.Turn));
				if (Segment.IsStraight())
				{
					LongestStraight = FMath::Max(LongestStraight, Segment.Length);
					continue;
				}
				bFastCorner |= Segment.Radius() >= 150.0 && Degrees >= 60.0;
				bSlowCorner |= Segment.Radius() <= 40.0 && Degrees >= 80.0 && Degrees <= 100.0;
				bHairpin |= Segment.Radius() <= 20.0 && Degrees >= 170.0;
				// Left-right-left (or the other way) with no straight in between.
				if (Index + 2 < Segments.Num() && !Segments[Index + 1].IsStraight() && !Segments[Index + 2].IsStraight())
				{
					bChicane |= Segment.Turn * Segments[Index + 1].Turn < 0.0 && Segments[Index + 1].Turn * Segments[Index + 2].Turn < 0.0;
				}
			}
			TestTrue(FString::Printf(TEXT("a straight of at least 400 m (longest %.0f m)"), LongestStraight), LongestStraight >= 400.0);
			TestTrue(TEXT("a fast corner"), bFastCorner);
			TestTrue(TEXT("a slow 90-degree corner"), bSlowCorner);
			TestTrue(TEXT("a hairpin"), bHairpin);
			TestTrue(TEXT("a chicane"), bChicane);
		});

		It("lines every corner with curbs and keeps the straights clear", [this]()
		{
			const FGTOTrackLayout Layout = FGTOTrackLayout::TestTrack();
			for (const int32 Corner : Corners(Layout))
			{
				TestEqual(FString::Printf(TEXT("full curb in the middle of corner %d"), Corner), Layout.CurbWeightAt(Mid(Layout, Corner)), 1.0);
			}
			TestEqual(TEXT("no curb half way down the main straight"), Layout.CurbWeightAt(Mid(Layout, 0)), 0.0);
		});

		It("ramps its curbs in and out instead of starting them with a step", [this]()
		{
			const FGTOTrackLayout Layout = FGTOTrackLayout::TestTrack();
			constexpr double Step = 0.1;
			double WorstChange = 0.0;
			for (double Distance = 0.0; Distance < Layout.GetLength(); Distance += Step)
			{
				WorstChange = FMath::Max(WorstChange, FMath::Abs(Layout.CurbWeightAt(Distance + Step) - Layout.CurbWeightAt(Distance)));
			}
			// The full curb height over 0.1 m of track at most a few millimetres apart.
			TestTrue(FString::Printf(TEXT("curb weight changes at most 0.05 per 0.1 m (worst %g)"), WorstChange), WorstChange <= 0.05);
		});

		It("puts the start line on the main straight with a long run to the first corner", [this]()
		{
			const FGTOTrackLayout Layout = FGTOTrackLayout::TestTrack();
			const double Line = Layout.GetStartLineDistance();
			const FGTOTrackSegment& MainStraight = Layout.GetSegments()[0];
			TestTrue(TEXT("on the first segment, a straight"), MainStraight.IsStraight() && Line > 0.0 && Line < MainStraight.Length);
			TestTrue(FString::Printf(TEXT("%.0f m to the first corner"), MainStraight.Length - Line), MainStraight.Length - Line >= 200.0);
			TestEqual(TEXT("no curb at the line"), Layout.CurbWeightAt(Line), 0.0);
		});
	});

	Describe("the cross-section", [this]()
	{
		It("keeps the road flat and the grass below it", [this]()
		{
			const FGTOTrackCrossSection Section;
			TestEqual(TEXT("road centre"), Section.HeightAt(0.0, 1.0), 0.0);
			TestEqual(TEXT("road edge"), Section.HeightAt(Section.HalfWidth, 1.0), 0.0);
			TestEqual(TEXT("grass beyond the verge"), Section.HeightAt(Section.VergeOuterOffset() + 1.0, 0.0), -Section.GrassDrop);
		});

		It("raises the curb only where the curb weight says so", [this]()
		{
			const FGTOTrackCrossSection Section;
			const double OnCurb = Section.HalfWidth + 0.5 * Section.CurbWidth;
			TestEqual(TEXT("full curb"), Section.HeightAt(OnCurb, 1.0), Section.CurbHeight, 1.0e-9);
			TestTrue(TEXT("no curb: the verge slopes to the grass"), Section.HeightAt(OnCurb, 0.0) < 0.0 && Section.HeightAt(OnCurb, 0.0) > -Section.GrassDrop);
		});

		It("has no steps across, so a wheel rolls over curb and verge", [this]()
		{
			const FGTOTrackCrossSection Section;
			for (const double Weight : { 0.0, 0.5, 1.0 })
			{
				constexpr double Step = 0.01;
				double WorstStep = 0.0;
				for (double Lateral = 0.0; Lateral < Section.VergeOuterOffset() + 1.0; Lateral += Step)
				{
					WorstStep = FMath::Max(WorstStep, FMath::Abs(Section.HeightAt(Lateral + Step, Weight) - Section.HeightAt(Lateral, Weight)));
				}
				TestTrue(FString::Printf(TEXT("curb weight %g: height changes at most 2 mm per cm (worst %g m)"), Weight, WorstStep), WorstStep <= 0.002);
			}
		});
	});
}

#endif
