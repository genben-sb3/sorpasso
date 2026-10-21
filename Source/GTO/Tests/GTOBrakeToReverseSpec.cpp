#include "Misc/AutomationTest.h"
#include "Vehicle/GTOBrakeToReverse.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float FrameDt = 1.f / 60.f; // s

	FGTODriverInput Pedals(float Throttle, float Brake)
	{
		FGTODriverInput Input;
		Input.Throttle = Throttle;
		Input.Brake = Brake;
		return Input;
	}

	/** Holds the pedals for Seconds at ForwardSpeed and returns the last input. */
	FGTODriverInput Hold(FGTOBrakeToReverse& Selector, const FGTODriverInput& Input, double ForwardSpeed, float Seconds)
	{
		FGTODriverInput Result;
		for (float Time = 0.f; Time < Seconds; Time += FrameDt)
		{
			Result = Selector.Apply(Input, ForwardSpeed, FrameDt);
		}
		return Result;
	}
}

BEGIN_DEFINE_SPEC(FGTOBrakeToReverseSpec, "GTO.Vehicle.BrakeToReverse", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
	FGTOBrakeToReverse Selector;
END_DEFINE_SPEC(FGTOBrakeToReverseSpec)

void FGTOBrakeToReverseSpec::Define()
{
	BeforeEach([this]()
	{
		Selector = FGTOBrakeToReverse();
	});

	It("drives forward with the pedals as pressed", [this]()
	{
		const FGTODriverInput Input = Selector.Apply(Pedals(0.7f, 0.2f), 10.0, FrameDt);
		TestEqual(TEXT("gear request"), Input.GearRequest, static_cast<int8>(1));
		TestEqual(TEXT("throttle"), Input.Throttle, 0.7f);
		TestEqual(TEXT("brake"), Input.Brake, 0.2f);
	});

	It("only brakes while the car still rolls", [this]()
	{
		const FGTODriverInput Input = Hold(Selector, Pedals(0.f, 1.f), 2.0, 2.f);
		TestEqual(TEXT("gear request"), Input.GearRequest, static_cast<int8>(1));
		TestEqual(TEXT("brake"), Input.Brake, 1.f);
	});

	It("engages reverse once the brake is held at a standstill, and swaps the pedals", [this]()
	{
		const FGTODriverInput Before = Hold(Selector, Pedals(0.f, 1.f), 0.0, 0.5f * FGTOBrakeToReverse::ReverseAfter);
		TestEqual(TEXT("still in drive at first"), Before.GearRequest, static_cast<int8>(1));

		const FGTODriverInput After = Hold(Selector, Pedals(0.f, 1.f), 0.0, FGTOBrakeToReverse::ReverseAfter);
		TestEqual(TEXT("gear request"), After.GearRequest, static_cast<int8>(-1));
		TestEqual(TEXT("the brake pedal drives backwards"), After.Throttle, 1.f);
		TestEqual(TEXT("and does not brake"), After.Brake, 0.f);

		const FGTODriverInput Backing = Selector.Apply(Pedals(0.3f, 0.f), -3.0, FrameDt);
		TestEqual(TEXT("the throttle pedal brakes while backing up"), Backing.Brake, 0.3f);
		TestEqual(TEXT("still in reverse while rolling backwards"), Backing.GearRequest, static_cast<int8>(-1));
	});

	It("engages drive again with throttle at a standstill", [this]()
	{
		Hold(Selector, Pedals(0.f, 1.f), 0.0, 2.f * FGTOBrakeToReverse::ReverseAfter);
		const FGTODriverInput Input = Selector.Apply(Pedals(1.f, 0.f), 0.0, FrameDt);
		TestEqual(TEXT("gear request"), Input.GearRequest, static_cast<int8>(1));
		TestEqual(TEXT("throttle"), Input.Throttle, 1.f);
	});

	It("is back in drive after a reset", [this]()
	{
		Hold(Selector, Pedals(0.f, 1.f), 0.0, 2.f * FGTOBrakeToReverse::ReverseAfter);
		Selector.Reset();
		TestFalse(TEXT("reversing"), Selector.IsReversing());
	});
}

#endif
