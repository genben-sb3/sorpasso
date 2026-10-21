#include "Vehicle/GTOBrakeToReverse.h"

#include "ultragravity/vehicle_step.h"

namespace
{
	// A pedal counts as pressed above this, as released below the other.
	constexpr float PedalPressed = 0.5f;
	constexpr float PedalReleased = 0.05f;
}

FGTODriverInput FGTOBrakeToReverse::Apply(FGTODriverInput Pedals, double ForwardSpeed, float DeltaSeconds)
{
	// Standing where the physics package lets reverse engage, in either direction.
	const bool bStanding = FMath::Abs(ForwardSpeed) < ultragravity::reverse_engage_speed;
	StandingOnBrake = bStanding && Pedals.Brake > PedalPressed && Pedals.Throttle < PedalReleased ? StandingOnBrake + DeltaSeconds : 0.f;
	if (!bReverse && StandingOnBrake >= ReverseAfter)
	{
		bReverse = true;
	}
	else if (bReverse && bStanding && Pedals.Throttle > PedalPressed && Pedals.Brake < PedalReleased)
	{
		bReverse = false;
	}

	FGTODriverInput Input = Pedals;
	Input.GearRequest = bReverse ? -1 : 1;
	if (bReverse)
	{
		Swap(Input.Throttle, Input.Brake);
	}
	return Input;
}

void FGTOBrakeToReverse::Reset()
{
	bReverse = false;
	StandingOnBrake = 0.f;
}
