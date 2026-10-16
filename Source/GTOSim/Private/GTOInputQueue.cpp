#include "GTOInputQueue.h"
#include "Misc/ScopeLock.h"

void FGTOInputQueue::Push(const FGTOInputPacket& Packet)
{
	FScopeLock ScopeLock(&Lock);
	Pending.Add(Packet);
}

FGTOInputPacket FGTOInputQueue::Pop(uint32 Tick)
{
	FScopeLock ScopeLock(&Lock);
	for (int32 Index = 0; Index < Pending.Num();)
	{
		if (Pending[Index].Tick <= Tick)
		{
			Last = Pending[Index]; // queued later means newer
			Pending.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
	Last.Tick = Tick;
	return Last;
}

void FGTOInputQueue::Reset()
{
	FScopeLock ScopeLock(&Lock);
	Pending.Reset();
	Last = FGTOInputPacket();
}
