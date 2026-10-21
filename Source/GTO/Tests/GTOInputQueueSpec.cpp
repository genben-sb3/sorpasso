#include "Misc/AutomationTest.h"
#include "Vehicle/GTOInputQueue.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGTODriverInput Throttle(float Value)
	{
		FGTODriverInput Input;
		Input.Throttle = Value;
		return Input;
	}
}

BEGIN_DEFINE_SPEC(FGTOInputQueueSpec, "GTO.Vehicle.InputQueue", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOInputQueueSpec)

void FGTOInputQueueSpec::Define()
{
	It("gives neutral input before any input arrives", [this]()
	{
		FGTOInputQueue Queue;
		TestEqual(TEXT("throttle"), Queue.Pop(7).Throttle, 0.f);
	});

	It("gives each tick the input queued for it", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(0, Throttle(0.25f));
		Queue.Push(1, Throttle(0.5f));
		TestEqual(TEXT("tick 0"), Queue.Pop(0).Throttle, 0.25f);
		TestEqual(TEXT("tick 1"), Queue.Pop(1).Throttle, 0.5f);
	});

	It("repeats the last input on ticks without one", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(0, Throttle(0.75f));
		Queue.Pop(0);
		TestEqual(TEXT("throttle"), Queue.Pop(1).Throttle, 0.75f);
	});

	It("takes the newest of several inputs that are due", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(3, Throttle(0.1f));
		Queue.Push(4, Throttle(0.2f));
		Queue.Push(4, Throttle(0.3f));
		TestEqual(TEXT("throttle on tick 5"), Queue.Pop(5).Throttle, 0.3f);
		TestEqual(TEXT("nothing older comes back later"), Queue.Pop(6).Throttle, 0.3f);
	});

	It("keeps inputs for later ticks until their tick", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(0, Throttle(0.2f));
		Queue.Push(2, Throttle(0.9f));
		TestEqual(TEXT("tick 0"), Queue.Pop(0).Throttle, 0.2f);
		TestEqual(TEXT("tick 1 repeats tick 0"), Queue.Pop(1).Throttle, 0.2f);
		TestEqual(TEXT("tick 2"), Queue.Pop(2).Throttle, 0.9f);
	});

	It("drops queued input and returns to neutral on reset", [this]()
	{
		FGTOInputQueue Queue;
		Queue.Push(0, Throttle(0.5f));
		Queue.Pop(0);
		Queue.Push(1, Throttle(0.8f));
		Queue.Reset();
		TestEqual(TEXT("throttle after reset"), Queue.Pop(1).Throttle, 0.f);
	});
}

#endif
