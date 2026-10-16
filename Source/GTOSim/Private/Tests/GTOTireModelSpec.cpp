#include "Misc/AutomationTest.h"
#include "GTOTireModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGTOTireParams MakeTire()
	{
		FGTOTireParams Tire;
		Tire.NominalLoad = 4000.0;
		Tire.Longitudinal.PeakFriction = 1.1;
		Tire.Longitudinal.LoadSensitivity = -0.1;
		Tire.Longitudinal.PeakSlip = 0.1;
		Tire.Longitudinal.PeakSlipLoadSensitivity = 0.2;
		Tire.Longitudinal.Shape = 1.65;
		Tire.Longitudinal.Curvature = 0.5;
		Tire.Longitudinal.CombinedSlipStiffness = 1.0;
		Tire.Lateral.PeakFriction = 1.0;
		Tire.Lateral.LoadSensitivity = -0.1;
		Tire.Lateral.PeakSlip = 0.12;
		Tire.Lateral.PeakSlipLoadSensitivity = 0.2;
		Tire.Lateral.Shape = 1.3;
		Tire.Lateral.Curvature = -0.5;
		Tire.Lateral.CombinedSlipStiffness = 1.0;
		return Tire;
	}

	struct FPeak
	{
		double Slip = 0.0;
		double Force = 0.0;
	};

	// Largest force over a fine sweep of pure longitudinal slip (or pure slip angle) from 0 to 1.
	FPeak SweepPeak(const IGTOTireModel& Tire, double Load, bool bLateral, double Friction = 1.0)
	{
		FPeak Peak;
		for (int32 Step = 1; Step <= 100000; ++Step)
		{
			const double Slip = Step * 1.0e-5;
			const FGTOTireForces Forces = bLateral ? Tire.Evaluate(0.0, Slip, Load, Friction, 0.0) : Tire.Evaluate(Slip, 0.0, Load, Friction, 0.0);
			const double Force = bLateral ? Forces.Lateral : Forces.Longitudinal;
			if (Force > Peak.Force)
			{
				Peak = { Slip, Force };
			}
		}
		return Peak;
	}
}

BEGIN_DEFINE_SPEC(FGTOTireModelSpec, "GTO.Sim.Tire", EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)
END_DEFINE_SPEC(FGTOTireModelSpec)

void FGTOTireModelSpec::Define()
{
	Describe("At constant load", [this]()
	{
		It("follows the Magic Formula curve of a known coefficient set", [this]()
		{
			// MathWorks' dry asphalt set B = 10, C = 1.9, D = 1, E = 0.97: with C and E the curve peaks at B·κ = 1.80194,
			// so a peak slip of 0.180194 gives B = 10. Expected values worked out independently from F = Fz·sin(C·atan(Bκ − E(Bκ − atan Bκ))).
			FGTOTireParams Params = MakeTire();
			Params.Longitudinal.PeakFriction = 1.0;
			Params.Longitudinal.PeakSlip = 0.1801944;
			Params.Longitudinal.Shape = 1.9;
			Params.Longitudinal.Curvature = 0.97;
			const FGTOMagicFormulaTire Tire(Params);
			const double Load = Params.NominalLoad;

			TestNearlyEqual(TEXT("force at slip 0.02"), Tire.Evaluate(0.02, 0.0, Load, 1.0, 0.0).Longitudinal, 0.3620200 * Load, 1.0);
			TestNearlyEqual(TEXT("force at slip 0.05"), Tire.Evaluate(0.05, 0.0, Load, 1.0, 0.0).Longitudinal, 0.7356193 * Load, 1.0);
			TestNearlyEqual(TEXT("force at slip 0.2"), Tire.Evaluate(0.2, 0.0, Load, 1.0, 0.0).Longitudinal, 0.9991777 * Load, 1.0);
			TestNearlyEqual(TEXT("force at slip 1 (locked)"), Tire.Evaluate(1.0, 0.0, Load, 1.0, 0.0).Longitudinal, 0.9145220 * Load, 1.0);
			TestNearlyEqual(TEXT("braking mirrors driving"), Tire.Evaluate(-0.05, 0.0, Load, 1.0, 0.0).Longitudinal, -0.7356193 * Load, 1.0);
		});

		It("peaks at its peak friction times the load, at the peak slip it reports for that load", [this]()
		{
			// 5000 N is 1.25 times the nominal 4000 N: friction 1.1 · 1.25^−0.1 = 1.075726, so the peak is 5378.63 N
			// longitudinally and 1.0 · 1.25^−0.1 · 5000 = 4889.66 N laterally; peak slips 0.1 · 1.05 and 0.12 · 1.05.
			const FGTOMagicFormulaTire Tire(MakeTire());
			const double Load = 5000.0;

			const FPeak Longitudinal = SweepPeak(Tire, Load, false);
			TestNearlyEqual(TEXT("longitudinal peak force (N)"), Longitudinal.Force, 5378.63, 0.5);
			TestNearlyEqual(TEXT("longitudinal peak slip"), Longitudinal.Slip, 0.105, 1.0e-4);
			TestNearlyEqual(TEXT("reported peak slip ratio"), Tire.PeakSlipRatio(Load), 0.105, 1.0e-9);

			const FPeak Lateral = SweepPeak(Tire, Load, true);
			TestNearlyEqual(TEXT("lateral peak force (N)"), Lateral.Force, 4889.66, 0.5);
			TestNearlyEqual(TEXT("lateral peak slip angle (rad)"), Lateral.Slip, 0.126, 1.0e-4);
			TestNearlyEqual(TEXT("reported peak slip angle"), Tire.PeakSlipAngle(Load), 0.126, 1.0e-9);
		});

		It("scales its grip with the surface friction", [this]()
		{
			const FGTOMagicFormulaTire Tire(MakeTire());
			TestNearlyEqual(TEXT("peak on a surface with half the grip (N)"), SweepPeak(Tire, 5000.0, false, 0.5).Force, 0.5 * 5378.63, 0.5);
		});
	});

	Describe("Under more load", [this]()
	{
		It("gives more force, but less than in proportion", [this]()
		{
			// Twice the nominal load: friction 1.1 · 2^−0.1 = 1.026336, so 8210.69 N instead of 2 · 4400 N.
			const FGTOMagicFormulaTire Tire(MakeTire());
			const double Nominal = SweepPeak(Tire, 4000.0, false).Force;
			const double Double = SweepPeak(Tire, 8000.0, false).Force;

			TestNearlyEqual(TEXT("peak at nominal load (N)"), Nominal, 4400.0, 0.5);
			TestNearlyEqual(TEXT("peak at twice the load (N)"), Double, 8210.69, 0.5);
			TestTrue(TEXT("more load, more force"), Double > Nominal);
			TestTrue(TEXT("less than twice the force"), Double < 2.0 * Nominal);

			const double NominalLateral = SweepPeak(Tire, 4000.0, true).Force;
			const double DoubleLateral = SweepPeak(Tire, 8000.0, true).Force;
			TestTrue(FString::Printf(TEXT("lateral: %g N at twice the load, between %g N and twice that"), DoubleLateral, NominalLateral),
				DoubleLateral > NominalLateral && DoubleLateral < 2.0 * NominalLateral);
		});
	});

	Describe("Under extreme load, as on a bump stop or a hard landing", [this]()
	{
		It("still gives more force for more load", [this]()
		{
			const FGTOMagicFormulaTire Tire(MakeTire());
			double PreviousPeak = 0.0;
			double PreviousFriction = TNumericLimits<double>::Max();
			for (const double Load : { 4000.0, 10000.0, 20000.0, 40000.0, 80000.0 })
			{
				const double Peak = SweepPeak(Tire, Load, false).Force;
				TestTrue(FString::Printf(TEXT("peak at %g N (%g N) above the one at the lower load (%g N)"), Load, Peak, PreviousPeak), Peak > PreviousPeak);
				TestTrue(FString::Printf(TEXT("peak per load at %g N (%g) below the one at the lower load (%g)"), Load, Peak / Load, PreviousFriction), Peak / Load < PreviousFriction);
				PreviousPeak = Peak;
				PreviousFriction = Peak / Load;
			}
		});
	});

	Describe("Under combined slip", [this]()
	{
		It("loses side force as the tyre also brakes or drives", [this]()
		{
			const FGTOMagicFormulaTire Tire(MakeTire());
			const double Load = 4000.0;
			const double SlipAngle = Tire.PeakSlipAngle(Load);

			const double Pure = Tire.Evaluate(0.0, SlipAngle, Load, 1.0, 0.0).Lateral;
			const double Driving = Tire.Evaluate(0.1, SlipAngle, Load, 1.0, 0.0).Lateral;
			const double Braking = Tire.Evaluate(-0.1, SlipAngle, Load, 1.0, 0.0).Lateral;
			const double Locked = Tire.Evaluate(-1.0, SlipAngle, Load, 1.0, 0.0).Lateral;

			TestTrue(FString::Printf(TEXT("driving at slip 0.1 (%g N) below pure cornering (%g N)"), Driving, Pure), Driving < Pure && Driving > 0.0);
			TestTrue(FString::Printf(TEXT("braking at slip 0.1 (%g N) below pure cornering (%g N)"), Braking, Pure), Braking < Pure && Braking > 0.0);
			TestTrue(FString::Printf(TEXT("locked wheel (%g N) keeps under a fifth of it"), Locked), Locked < 0.2 * Pure);
		});

		It("loses drive force as the tyre also corners", [this]()
		{
			const FGTOMagicFormulaTire Tire(MakeTire());
			const double Load = 4000.0;
			const double SlipRatio = Tire.PeakSlipRatio(Load);

			const double Pure = Tire.Evaluate(SlipRatio, 0.0, Load, 1.0, 0.0).Longitudinal;
			const double Cornering = Tire.Evaluate(SlipRatio, 0.1, Load, 1.0, 0.0).Longitudinal;
			TestTrue(FString::Printf(TEXT("at slip angle 0.1 rad (%g N) below straight (%g N)"), Cornering, Pure), Cornering < Pure && Cornering > 0.0);
		});
	});
}

#endif
