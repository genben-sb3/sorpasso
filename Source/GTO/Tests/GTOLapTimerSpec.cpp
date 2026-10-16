#include "Misc/AutomationTest.h"
#include "Game/GTOLapTimer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Gate across a 12 m wide road along +X at x = 0. */
	FGTOTimingGate Gate()
	{
		FGTOTimingGate Result;
		Result.Centre = FVector::ZeroVector;
		Result.Forward = FVector::ForwardVector;
		Result.HalfWidth = 6.0;
		Result.HalfHeight = 3.0;
		return Result;
	}

	FVector At(double X, double Y = 0.0)
	{
		return FVector(X, Y, 0.5);
	}

	/** Drives a lap of LapSeconds: crosses the gate at 10 m/s, goes round and crosses again exactly LapSeconds later. */
	void DriveLap(FGTOLapTimer& Timer, double StartTime, double LapSeconds)
	{
		Timer.Update(StartTime, At(-5.0));
		Timer.Update(StartTime + 1.0, At(5.0)); // crosses at StartTime + 0.5
		Timer.Update(StartTime + 0.5 * LapSeconds, At(50.0, 300.0)); // far side of the track
		Timer.Update(StartTime + LapSeconds, At(-5.0)); // back round, passing the line plane well beside the gate
		Timer.Update(StartTime + LapSeconds + 1.0, At(5.0)); // crosses at StartTime + LapSeconds + 0.5
	}
}

BEGIN_DEFINE_SPEC(FGTOLapTimerSpec, "GTO.Game.LapTimer", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOLapTimerSpec)

void FGTOLapTimerSpec::Define()
{
	It("shows no times before the car crosses the line", [this]()
	{
		FGTOLapTimer Timer(Gate());
		Timer.Update(0.0, At(-10.0));
		Timer.Update(1.0, At(-1.0));
		TestFalse(TEXT("current"), Timer.GetCurrentLapTime().IsSet());
		TestFalse(TEXT("last"), Timer.GetLastLapTime().IsSet());
		TestFalse(TEXT("best"), Timer.GetBestLapTime().IsSet());
	});

	It("starts the lap at the moment the car crosses the line, between two updates", [this]()
	{
		FGTOLapTimer Timer(Gate());
		Timer.Update(10.0, At(-2.0));
		Timer.Update(14.0, At(6.0)); // 2 m of 8 m behind the line: crossed at 11 s
		TestEqual(TEXT("current lap time"), Timer.GetCurrentLapTime().Get(-1.0), 3.0, 1.0e-9);
		Timer.Update(15.0, At(16.0));
		TestEqual(TEXT("current lap time a second later"), Timer.GetCurrentLapTime().Get(-1.0), 4.0, 1.0e-9);
		TestFalse(TEXT("no lap completed yet"), Timer.GetLastLapTime().IsSet());
	});

	It("completes the lap on the next crossing and starts the next one", [this]()
	{
		FGTOLapTimer Timer(Gate());
		DriveLap(Timer, 0.0, 70.0);
		TestEqual(TEXT("last"), Timer.GetLastLapTime().Get(-1.0), 70.0, 1.0e-9);
		TestEqual(TEXT("best"), Timer.GetBestLapTime().Get(-1.0), 70.0, 1.0e-9);
		TestEqual(TEXT("current lap started at the crossing"), Timer.GetCurrentLapTime().Get(-1.0), 0.5, 1.0e-9);
	});

	It("keeps the fastest lap as the best and the latest as the last", [this]()
	{
		FGTOLapTimer Timer(Gate());
		DriveLap(Timer, 0.0, 70.0);
		DriveLap(Timer, 100.0, 65.0);
		DriveLap(Timer, 200.0, 68.0);
		TestEqual(TEXT("last"), Timer.GetLastLapTime().Get(-1.0), 68.0, 1.0e-9);
		TestEqual(TEXT("best"), Timer.GetBestLapTime().Get(-1.0), 65.0, 1.0e-9);
	});

	It("ignores a crossing beside the gate", [this]()
	{
		FGTOLapTimer Timer(Gate());
		Timer.Update(0.0, At(-5.0, 20.0));
		Timer.Update(1.0, At(5.0, 20.0));
		TestFalse(TEXT("current"), Timer.GetCurrentLapTime().IsSet());
	});

	It("ignores a crossing above or below the gate", [this]()
	{
		FGTOLapTimer Timer(Gate());
		Timer.Update(0.0, FVector(-5.0, 0.0, 10.0));
		Timer.Update(1.0, FVector(5.0, 0.0, 10.0));
		TestFalse(TEXT("current"), Timer.GetCurrentLapTime().IsSet());
	});

	It("drops the lap in progress when the car crosses the line backwards", [this]()
	{
		FGTOLapTimer Timer(Gate());
		Timer.Update(0.0, At(-5.0));
		Timer.Update(1.0, At(5.0));
		Timer.Update(2.0, At(-5.0)); // reversed back over the line
		TestFalse(TEXT("no lap in progress"), Timer.GetCurrentLapTime().IsSet());

		Timer.Update(3.0, At(5.0)); // and forwards again a second later
		TestFalse(TEXT("no lap completed"), Timer.GetLastLapTime().IsSet());
		TestEqual(TEXT("a new lap runs from the crossing"), Timer.GetCurrentLapTime().Get(-1.0), 0.5, 1.0e-9);
	});

	It("discards the lap in progress on a reset and counts no lap until the line is crossed again", [this]()
	{
		FGTOLapTimer Timer(Gate());
		DriveLap(Timer, 0.0, 70.0);
		Timer.Update(80.0, At(-5.0, 300.0));
		Timer.Discard();
		TestFalse(TEXT("no lap in progress after the reset"), Timer.GetCurrentLapTime().IsSet());

		// Put back behind the line and driven over it 10 s after the reset: a fresh start, not a 10 s lap.
		Timer.Update(81.0, At(-5.0));
		Timer.Update(91.0, At(5.0));
		TestEqual(TEXT("last lap unchanged"), Timer.GetLastLapTime().Get(-1.0), 70.0, 1.0e-9);
		TestEqual(TEXT("best lap unchanged"), Timer.GetBestLapTime().Get(-1.0), 70.0, 1.0e-9);
		TestEqual(TEXT("new lap from the crossing"), Timer.GetCurrentLapTime().Get(-1.0), 5.0, 1.0e-9);
	});

	It("does not count a jump across the line faster than any car drives, e.g. a teleport", [this]()
	{
		FGTOLapTimer Timer(Gate());
		Timer.Update(0.0, At(-500.0));
		Timer.Update(0.01, At(5.0));
		TestFalse(TEXT("no lap started"), Timer.GetCurrentLapTime().IsSet());
	});
}

#endif
