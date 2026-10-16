#include "GTOTireModel.h"

namespace
{
	/** B·s at the peak of sin(C·atan(B·s − E·(B·s − atan(B·s)))): where the inner term reaches tan(π/(2C)). */
	double FindPeakInput(double Shape, double Curvature)
	{
		const double Target = FMath::Tan(UE_DOUBLE_HALF_PI / Shape);
		const auto Inner = [Curvature](double X) { return X - Curvature * (X - FMath::Atan(X)); };

		// The inner term grows monotonically for E <= 1, so bisection finds the single crossing.
		double Low = 0.0;
		double High = 1.0;
		while (Inner(High) < Target && High < 1.0e6)
		{
			High *= 2.0;
		}
		for (int32 Iteration = 0; Iteration < 60; ++Iteration)
		{
			const double Mid = 0.5 * (Low + High);
			(Inner(Mid) < Target ? Low : High) = Mid;
		}
		return 0.5 * (Low + High);
	}

	/** Weighting of one direction's force by the other direction's slip, 1 without it. */
	double CombinedWeight(double OtherSlip, double OtherPeakSlip, double Stiffness)
	{
		return FMath::Cos(FMath::Atan(Stiffness * OtherSlip / OtherPeakSlip));
	}
}

double FGTOMagicFormulaTire::FCurve::PeakSlip(double LoadChange) const
{
	return FMath::Max(1.0e-4, Params.PeakSlip * (1.0 + Params.PeakSlipLoadSensitivity * LoadChange));
}

double FGTOMagicFormulaTire::FCurve::Force(double Slip, double GripLoad, double LoadChange) const
{
	const double D = Params.PeakFriction * FMath::Pow(1.0 + LoadChange, Params.LoadSensitivity) * GripLoad;
	const double BS = PeakInput / PeakSlip(LoadChange) * Slip;
	const double E = Params.Curvature;
	return D * FMath::Sin(Params.Shape * FMath::Atan(BS - E * (BS - FMath::Atan(BS))));
}

FGTOMagicFormulaTire::FGTOMagicFormulaTire(const FGTOTireParams& InParams)
	: Params(InParams)
{
	const auto MakeCurve = [](const FGTOTireCurveParams& CurveParams)
	{
		FCurve Curve;
		Curve.Params = CurveParams;
		Curve.Params.Shape = FMath::Clamp(Curve.Params.Shape, 1.01, 1.99); // C <= 1 has no peak
		Curve.Params.Curvature = FMath::Min(Curve.Params.Curvature, 1.0);
		Curve.Params.LoadSensitivity = FMath::Clamp(Curve.Params.LoadSensitivity, -0.99, 0.0); // below −1 more load would mean less force
		Curve.PeakInput = FindPeakInput(Curve.Params.Shape, Curve.Params.Curvature);
		return Curve;
	};
	Longitudinal = MakeCurve(Params.Longitudinal);
	Lateral = MakeCurve(Params.Lateral);
}

FGTOTireForces FGTOMagicFormulaTire::Evaluate(double SlipRatio, double SlipAngle, double Load, double Friction, double Camber) const
{
	FGTOTireForces Forces;
	if (Load <= 0.0 || Friction <= 0.0)
	{
		return Forces;
	}

	const double Change = LoadChange(Load);
	const double GripLoad = Load * Friction;
	Forces.Longitudinal = Longitudinal.Force(SlipRatio, GripLoad, Change)
		* CombinedWeight(SlipAngle, Lateral.PeakSlip(Change), Longitudinal.Params.CombinedSlipStiffness);
	Forces.Lateral = Lateral.Force(SlipAngle, GripLoad, Change)
		* CombinedWeight(SlipRatio, Longitudinal.PeakSlip(Change), Lateral.Params.CombinedSlipStiffness);
	return Forces;
}

double FGTOMagicFormulaTire::PeakSlipRatio(double Load) const
{
	return Longitudinal.PeakSlip(LoadChange(Load));
}

double FGTOMagicFormulaTire::PeakSlipAngle(double Load) const
{
	return Lateral.PeakSlip(LoadChange(Load));
}

double FGTOMagicFormulaTire::LoadChange(double Load) const
{
	return (Load - Params.NominalLoad) / Params.NominalLoad;
}
