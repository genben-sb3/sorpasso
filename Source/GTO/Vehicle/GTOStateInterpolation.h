#pragma once

#include "CoreMinimal.h"
#include "Vehicle/GTOVehicleTypes.h"

/** Blends two snapshots: Alpha 0 gives From, 1 gives To. Orientation follows the shortest arc. */
GTO_API FGTOVehicleState GTOInterpolateState(const FGTOVehicleState& From, const FGTOVehicleState& To, double Alpha);

/**
 * Display state between the last two sim snapshots. The display runs one sim step behind the latest snapshot and
 * follows frame time in between, so it moves smoothly while the sim steps at a fixed rate. Per frame: Push every
 * snapshot the frame's sim steps produced, then Advance by the frame time, then Sample.
 */
class GTO_API FGTOSnapshotInterpolator
{
public:
	explicit FGTOSnapshotInterpolator(double InStepDt = 1.0 / 120.0);

	/** Starts over from a single snapshot. */
	void Reset(const FGTOVehicleState& State, double InStepDt);

	void Push(const FGTOVehicleState& State);
	void Advance(double FrameDt);
	FGTOVehicleState Sample() const;

	/** Where Sample stands between the previous and the latest snapshot: 0 at the previous, 1 at the latest. */
	double GetAlpha() const;

private:
	FGTOVehicleState Previous;
	FGTOVehicleState Latest;
	double StepDt;
	double Remainder = 0.0; // frame time not yet covered by sim steps, in [0, StepDt] after Advance
};
