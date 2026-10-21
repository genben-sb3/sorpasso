#pragma once

#include "CoreMinimal.h"
#include "Vehicle/GTOVehicleTypes.h"

/**
 * Brake to reverse, as in most racing games with an automatic: holding the brake at a standstill engages reverse,
 * pressing the throttle at a standstill engages drive again. In reverse the pedals swap, so the brake pedal drives
 * backwards and the throttle pedal brakes. Only for a human driver; test drivers ask for the gear themselves.
 */
class GTO_API FGTOBrakeToReverse
{
public:
	/** How long the brake has to be held at a standstill before reverse engages (s). */
	static constexpr float ReverseAfter = 0.4f;

	/**
	 * Driver input with the pedals as pressed → the same with the gear request and the pedals the car needs.
	 * ForwardSpeed is the car's speed along its heading (m/s), DeltaSeconds the time since the last call.
	 */
	FGTODriverInput Apply(FGTODriverInput Pedals, double ForwardSpeed, float DeltaSeconds);

	/** Back to drive, e.g. after the car was reset. */
	void Reset();

	bool IsReversing() const { return bReverse; }

private:
	bool bReverse = false;
	float StandingOnBrake = 0.f; // s the brake has been held at a standstill without throttle
};
