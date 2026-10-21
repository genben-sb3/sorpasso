#pragma once

// The game side and the physics package work in metres, Unreal in centimetres.
namespace GTOUnits
{
	inline constexpr double CmPerM = 100.0;
	inline constexpr double Cm2PerM2 = CmPerM * CmPerM;
	inline constexpr double KmhPerMps = 3.6;
	inline constexpr double RpmPerRadPerSecond = 60.0 / UE_DOUBLE_TWO_PI;
}
