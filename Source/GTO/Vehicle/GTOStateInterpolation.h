#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Vehicle/GTOVehicleTypes.h"

/** Blends two snapshots: Alpha 0 gives From, 1 gives To. Orientation follows the shortest arc. */
GTO_API FGTOVehicleState GTOInterpolateState(const FGTOVehicleState& From, const FGTOVehicleState& To, double Alpha);

/**
 * Display state between the sim snapshots. The sim delivers one snapshot per fixed step, but not in lockstep with the
 * frames: a frame may bring none, one or several, and a frame's steps may arrive a frame late. The shown time therefore
 * follows frame time and is only pulled softly towards a fixed delay behind the newest snapshot; counting the arrivals
 * instead would make the car jump whenever they come unevenly. Per frame: Push every snapshot that arrived, then Advance
 * by the frame time, then Sample.
 */
class GTO_API FGTOSnapshotInterpolator
{
public:
	/** Snapshots kept: the delay plus a 60 fps frame's worth of late steps, with room to spare. */
	static constexpr int32 Capacity = 6;

	/** How far the display runs behind the newest snapshot, in sim steps. */
	static constexpr double Delay = 2.0;

	/** Time over which a deviation from that delay is worked off (s). Larger deviations are jumped. */
	static constexpr double CorrectionTime = 0.5;

	/** The two snapshots Sample blends, by their number since Reset (the Reset snapshot is 0), and the blend between them. */
	struct FBlend
	{
		int64 From = 0;
		int64 To = 0;
		double Alpha = 0.0; // 0 at From, 1 at To
	};

	explicit FGTOSnapshotInterpolator(double InStepDt = 1.0 / 120.0);

	/** Starts over from a single snapshot. */
	void Reset(const FGTOVehicleState& State, double InStepDt);

	void Push(const FGTOVehicleState& State);
	void Advance(double FrameDt);
	FGTOVehicleState Sample() const;
	FBlend GetBlend() const;

	/** Number of the newest snapshot since Reset. */
	int64 GetNewest() const { return Newest; }

	/** How far the shown time lies behind the newest snapshot, in sim steps. */
	double GetShownDelay() const { return static_cast<double>(Newest) - ShownTime(); }

	/** Ring slot of the snapshot with this number (0 or more), for data the caller keeps alongside the snapshots. */
	static int32 Slot(int64 Number) { return static_cast<int32>(Number % Capacity); }

private:
	/** The shown time within the snapshots still kept, in sim steps since Reset. */
	double ShownTime() const;

	TStaticArray<FGTOVehicleState, Capacity> Snapshots; // by Slot of their number
	double StepDt;
	int64 Newest = 0;
	double Shown = 0.0; // sim steps since Reset
};
