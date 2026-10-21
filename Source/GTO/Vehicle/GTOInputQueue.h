#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Vehicle/GTOVehicleTypes.h"

/**
 * Hands driver input from its producer (the game thread) to the sim tick. Each sim tick takes exactly one input: the
 * newest one queued for that tick or earlier. Without one, the previous input repeats. Inputs for later ticks wait for
 * their tick. Safe for one producer and one consumer on different threads.
 */
class GTO_API FGTOInputQueue
{
public:
	void Push(uint32 Tick, const FGTODriverInput& Input);

	/** The input to apply on Tick. */
	FGTODriverInput Pop(uint32 Tick);

	/** Drops all queued input and returns to neutral input. */
	void Reset();

private:
	struct FPending
	{
		uint32 Tick = 0;
		FGTODriverInput Input;
	};

	FCriticalSection Lock;
	TArray<FPending> Pending;
	FGTODriverInput Last;
};
