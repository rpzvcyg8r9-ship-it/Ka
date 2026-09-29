#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEisenwal, Log, All);

/** Umrechnungen, die im ganzen Projekt gebraucht werden (Unreal rechnet in cm). */
namespace Eisenwal
{
	constexpr float KnotsToCmPerSec = 51.4444f;   // 1 kn = 0,514444 m/s
	constexpr float CmPerSecToKnots = 1.f / KnotsToCmPerSec;
	constexpr float Gravity = 980.f;               // cm/s^2
	constexpr float WaterDensity = 0.001025f;      // kg/cm^3 (Salzwasser)
}
