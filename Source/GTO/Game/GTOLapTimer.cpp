#include "Game/GTOLapTimer.h"

FGTOLapTimer::FGTOLapTimer(const FGTOTimingGate& InGate)
	: Gate(InGate)
{
	Gate.Forward = FVector(Gate.Forward.X, Gate.Forward.Y, 0.0).GetSafeNormal();
}

void FGTOLapTimer::Update(double Time, const FVector& Position)
{
	const TOptional<double> FromTime = PreviousTime;
	const FVector From = PreviousPosition;
	PreviousTime = Time;
	PreviousPosition = Position;
	if (!FromTime || Time <= *FromTime)
	{
		return;
	}
	if (FVector::Dist(From, Position) > MaxSpeed * (Time - *FromTime))
	{
		return; // teleported
	}

	// Signed distances ahead of the line plane; a crossing is a sign change within the gate.
	const double Before = FVector::DotProduct(From - Gate.Centre, Gate.Forward);
	const double After = FVector::DotProduct(Position - Gate.Centre, Gate.Forward);
	const bool bForward = Before < 0.0 && After >= 0.0;
	const bool bBackward = Before >= 0.0 && After < 0.0;
	if (!bForward && !bBackward)
	{
		return;
	}

	const double Fraction = Before / (Before - After);
	const FVector Crossing = FMath::Lerp(From, Position, Fraction) - Gate.Centre;
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Gate.Forward);
	if (FMath::Abs(FVector::DotProduct(Crossing, Across)) > Gate.HalfWidth || FMath::Abs(Crossing.Z) > Gate.HalfHeight)
	{
		return;
	}

	if (bBackward)
	{
		LapStart.Reset();
		return;
	}

	const double CrossingTime = FMath::Lerp(*FromTime, Time, Fraction);
	if (LapStart)
	{
		LastLap = CrossingTime - *LapStart;
		BestLap = BestLap ? FMath::Min(*BestLap, *LastLap) : *LastLap;
	}
	LapStart = CrossingTime;
}

void FGTOLapTimer::Discard()
{
	LapStart.Reset();
	PreviousTime.Reset();
}

TOptional<double> FGTOLapTimer::GetCurrentLapTime() const
{
	if (!LapStart || !PreviousTime)
	{
		return {};
	}
	return *PreviousTime - *LapStart;
}
