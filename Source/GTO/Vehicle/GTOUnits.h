#pragma once

// The sim core works in metres, Unreal in centimetres.
namespace GTOUnits
{
	inline constexpr double CmPerM = 100.0;
	inline constexpr double Cm2PerM2 = CmPerM * CmPerM;
}
