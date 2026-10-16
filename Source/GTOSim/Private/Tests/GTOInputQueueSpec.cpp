#include "Misc/AutomationTest.h"
#include "GTOInputQueue.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGTOInputPacket Throttle(uint32 Tick, float Value)
	{
		FGTOInputPacket Packet;
		Packet.Tick = Tick;
		Packet.Throttle = Value;
		return Packet;
	}
}

BEGIN_DEFINE_SPEC(FGTOInputQueueSpec, "GTO.Sim.InputQueue", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOInputQueueSpec)

void FGTOInputQueueSpec::Define()
{
	It("gives neutral controls before any packet arrives, stamped with the tick", [this]()
	{
		FGTOInputQueue Queue;
		const FGTOInputPacket Packet = Queue.Pop(7);
		TestEqual(TEXT("tick"), Packet.Tick, 7u);
		TestEqual(TEXT("throttle"), Packet.Throttle, 0.f);
	});

	It("gives each tick the packet queued for it", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(Throttle(0, 0.25f));
		Queue.Push(Throttle(1, 0.5f));
		TestEqual(TEXT("tick 0"), Queue.Pop(0).Throttle, 0.25f);
		TestEqual(TEXT("tick 1"), Queue.Pop(1).Throttle, 0.5f);
	});

	It("repeats the last packet on ticks without one", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(Throttle(0, 0.75f));
		Queue.Pop(0);
		const FGTOInputPacket Repeated = Queue.Pop(1);
		TestEqual(TEXT("throttle"), Repeated.Throttle, 0.75f);
		TestEqual(TEXT("stamped with the new tick"), Repeated.Tick, 1u);
	});

	It("takes the newest of several packets that are due", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(Throttle(3, 0.1f));
		Queue.Push(Throttle(4, 0.2f));
		Queue.Push(Throttle(4, 0.3f));
		TestEqual(TEXT("throttle on tick 5"), Queue.Pop(5).Throttle, 0.3f);
		TestEqual(TEXT("nothing older comes back later"), Queue.Pop(6).Throttle, 0.3f);
	});

	It("keeps packets for later ticks until their tick", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(Throttle(0, 0.2f));
		Queue.Push(Throttle(2, 0.9f));
		TestEqual(TEXT("tick 0"), Queue.Pop(0).Throttle, 0.2f);
		TestEqual(TEXT("tick 1 repeats tick 0"), Queue.Pop(1).Throttle, 0.2f);
		TestEqual(TEXT("tick 2"), Queue.Pop(2).Throttle, 0.9f);
	});

	It("drops queued packets and returns to neutral on reset", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(Throttle(0, 0.5f));
		Queue.Pop(0);
		Queue.Push(Throttle(1, 0.8f));
		Queue.Reset();
		TestEqual(TEXT("throttle after reset"), Queue.Pop(1).Throttle, 0.f);
	});
}

#endif
