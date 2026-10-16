#pragma once

#include "CoreMinimal.h"

/** Start-finish line as a vertical gate, in metres: crossing it in Forward direction within its width and height counts. */
struct FGTOTimingGate
{
	FVector Centre = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector; // driving direction, horizontal unit vector
	double HalfWidth = 0.0;
	double HalfHeight = 0.0;
};

/**
 * Lap times of one car for the session, from its sim positions and sim time. A lap runs from one forward crossing of
 * the gate to the next; the crossing time is interpolated between two updates, so the times do not depend on the
 * frame rate. A reset or crossing the line backwards drops the lap in progress. Nothing is stored.
 */
class GTO_API FGTOLapTimer
{
public:
	/** Faster than any car: a jump between two updates beyond this speed is a teleport, not driving. */
	static constexpr double MaxSpeed = 200.0; // m/s

	explicit FGTOLapTimer(const FGTOTimingGate& InGate);

	/** The car's position (m) at sim time Time (s). Times must not go backwards. */
	void Update(double Time, const FVector& Position);

	/** The car was put somewhere else, e.g. reset to the start: the lap in progress does not count. */
	void Discard();

	/** Time since the lap in progress began, up to the last update. Unset while no lap is running. */
	TOptional<double> GetCurrentLapTime() const;
	TOptional<double> GetLastLapTime() const { return LastLap; }
	TOptional<double> GetBestLapTime() const { return BestLap; }

	const FGTOTimingGate& GetGate() const { return Gate; }

private:
	FGTOTimingGate Gate;

	TOptional<double> PreviousTime;
	FVector PreviousPosition = FVector::ZeroVector;

	TOptional<double> LapStart;
	TOptional<double> LastLap;
	TOptional<double> BestLap;
};
