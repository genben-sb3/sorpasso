#include "Vehicle/GTOInputQueue.h"

#include "Misc/ScopeLock.h"

void FGTOInputQueue::Push(uint32 Tick, const FGTODriverInput& Input)
{
	FScopeLock ScopeLock(&Lock);
	Pending.Add({ Tick, Input });
}

FGTODriverInput FGTOInputQueue::Pop(uint32 Tick)
{
	FScopeLock ScopeLock(&Lock);
	for (int32 Index = 0; Index < Pending.Num();)
	{
		if (Pending[Index].Tick <= Tick)
		{
			Last = Pending[Index].Input; // queued later means newer
			Pending.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
	return Last;
}

void FGTOInputQueue::Reset()
{
	FScopeLock ScopeLock(&Lock);
	Pending.Reset();
	Last = FGTODriverInput();
}
