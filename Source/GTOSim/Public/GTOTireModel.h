#pragma once

#include "CoreMinimal.h"

/**
 * One direction (longitudinal or lateral) of a Magic Formula tyre, F = D·sin(C·atan(B·s − E·(B·s − atan(B·s)))).
 * D and B follow from the peak friction and the peak slip at the given load (Pac94 style: both change with load).
 * The peak friction follows (load / nominal load)^LoadSensitivity, so the peak force keeps growing with load, just
 * less than in proportion, however high the load gets.
 */
struct FGTOTireCurveParams
{
	double PeakFriction = 1.0;            // D at nominal load: peak force / load
	double LoadSensitivity = 0.0;         // exponent of the peak friction over relative load, in (−1, 0] for real tyres
	double PeakSlip = 0.1;                // slip at the peak at nominal load (longitudinal: slip ratio, lateral: rad)
	double PeakSlipLoadSensitivity = 0.0; // relative change of the peak slip per relative load change
	double Shape = 1.5;                   // C, in (1, 2): lower keeps more force when sliding
	double Curvature = 0.0;               // E, at most 1: higher makes the peak flatter and the rise steeper
	double CombinedSlipStiffness = 1.0;   // B of the weighting cos(atan(B·s/s_peak)), s the other direction's slip
};

/** Tyre data. Slip ratio is (ωr − vx)/|vx|; slip angle in rad. */
struct FGTOTireParams
{
	double NominalLoad = 4000.0; // N
	FGTOTireCurveParams Longitudinal;
	FGTOTireCurveParams Lateral;
	double RelaxationLength = 0.4;    // m, rolling distance over which the slip angle builds up
	double RelaxationMinSpeed = 5.0;  // m/s: below, the slip angle acts at once, which keeps standstill and parking damped
	double RelaxationFullSpeed = 10.0; // m/s: from here on the slip angle lags fully; blended in from RelaxationMinSpeed
	double SlipMinSpeed = 2.0;        // m/s, VXLOW: the slip denominator saturates smoothly towards 2/3 of it at standstill
	double RollingResistance = 0.015; // rolling resistance force / load, braking the wheel's spin
};

/** Forces of the tyre in its contact plane: along the wheel heading and to its right. */
struct FGTOTireForces
{
	double Longitudinal = 0.0; // N
	double Lateral = 0.0;      // N
};

class IGTOTireModel
{
public:
	virtual ~IGTOTireModel() = default;

	/**
	 * Steady-state forces for the given slip. Positive slip ratio drives (force forward), positive slip angle means the
	 * tyre slides to its left and is pushed to its right. Friction is the surface grip, 1 for dry asphalt. Camber (rad)
	 * is part of the interface but not modelled yet; the sim passes 0.
	 */
	virtual FGTOTireForces Evaluate(double SlipRatio, double SlipAngle, double Load, double Friction, double Camber) const = 0;

	/** Slip ratio of the peak force under pure longitudinal slip at Load. */
	virtual double PeakSlipRatio(double Load) const = 0;

	/** Slip angle (rad) of the peak force under pure lateral slip at Load. */
	virtual double PeakSlipAngle(double Load) const = 0;
};

/** Magic Formula in Pac94 style: load sensitivity, combined slip through weighting functions. */
class GTOSIM_API FGTOMagicFormulaTire : public IGTOTireModel
{
public:
	explicit FGTOMagicFormulaTire(const FGTOTireParams& InParams);

	virtual FGTOTireForces Evaluate(double SlipRatio, double SlipAngle, double Load, double Friction, double Camber) const override;
	virtual double PeakSlipRatio(double Load) const override;
	virtual double PeakSlipAngle(double Load) const override;

private:
	struct FCurve
	{
		FGTOTireCurveParams Params;
		double PeakInput = 1.0; // B·s at the peak, fixed by C and E

		double PeakSlip(double LoadChange) const;
		double Force(double Slip, double GripLoad, double LoadChange) const;
	};

	double LoadChange(double Load) const;

	FGTOTireParams Params;
	FCurve Longitudinal;
	FCurve Lateral;
};
