#pragma once

#include <cmath>

#include "ultragravity/vehicle_file.h"

namespace ultragravity {

// Magic Formula over a slip, normalized to a peak value of 1, together with its
// derivative with respect to the slip.
struct CurvePoint {
    double value; // 1
    double slope; // 1 per unit of slip
};

inline CurvePoint magic_formula(const CurveShape& k, double slip) {
    const double b = k.stiffness;                                      // 1 per unit of slip
    const double bx = b * slip;                                        // 1
    const double phi = bx - k.curvature * (bx - std::atan(bx));        // 1
    const double dphi = b - k.curvature * (b - b / (1.0 + bx * bx));   // 1 per unit of slip
    const double angle = k.shape * std::atan(phi);                     // 1
    return {std::sin(angle), std::cos(angle) * k.shape / (1.0 + phi * phi) * dphi};
}

// Slip at which the curve reaches its peak (slope zero), for a curve whose
// peak lies below limit.
inline double peak_slip(const CurveShape& k, double limit) {
    double low = 0.0, high = limit; // same unit as the slip
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (low + high); // same unit as the slip
        (magic_formula(k, mid).slope > 0.0 ? low : high) = mid;
    }
    return 0.5 * (low + high);
}

} // namespace ultragravity
