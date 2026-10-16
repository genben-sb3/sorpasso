#pragma once

#include "GTOVehicleTypes.h"
#include "HAL/CriticalSection.h"

/**
 * Hands input packets from their producer (the game thread) to the sim tick. Each sim tick takes exactly one packet:
 * the newest one queued for that tick or earlier. Without one, the previous packet repeats. Packets for later
 * ticks wait for their tick. Safe for one producer and one consumer on different threads.
 */
class GTOSIM_API FGTOInputQueue
{
public:
	void Push(const FGTOInputPacket& Packet);

	/** The packet to apply on Tick, stamped with Tick. */
	FGTOInputPacket Pop(uint32 Tick);

	/** Drops all queued packets and returns to neutral controls. */
	void Reset();

private:
	FCriticalSection Lock;
	TArray<FGTOInputPacket> Pending;
	FGTOInputPacket Last;
};
