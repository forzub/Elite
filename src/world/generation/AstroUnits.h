#pragma once

namespace world::generation::astro
{

// SI base, IAU 2015 nominal values where defined.
inline constexpr double G = 6.67430e-11;
inline constexpr double Pi = 3.141592653589793;
inline constexpr double TwoPi = 6.283185307179586;
inline constexpr double DegToRad = Pi / 180.0;
inline constexpr double RadToDeg = 180.0 / Pi;

inline constexpr double AuMeters = 149597870700.0;
inline constexpr double LightYearMeters = 9460730472580800.0;
inline constexpr double ParsecLy = 3.261563777;
inline constexpr double DaySeconds = 86400.0;
inline constexpr double YearSeconds = 365.25 * DaySeconds;
inline constexpr double GyrSeconds = 1.0e9 * YearSeconds;

inline constexpr double SunMuM3s2 = 1.3271244e20;
inline constexpr double SunMassKg = SunMuM3s2 / G;
inline constexpr double SunRadiusM = 6.957e8;
inline constexpr double SunLuminosityW = 3.828e26;
inline constexpr double SunTeffK = 5772.0;

inline constexpr double EarthMassKg = 5.9722e24;
inline constexpr double EarthRadiusM = 6.3781e6;
inline constexpr double JupiterMassKg = 1.89813e27;
inline constexpr double JupiterRadiusM = 7.1492e7;

inline constexpr double EarthMassesPerSunMass = SunMassKg / EarthMassKg;      // ~332946
inline constexpr double EarthMassesPerJupiterMass = JupiterMassKg / EarthMassKg; // ~317.8

inline constexpr double StefanBoltzmann = 5.670374419e-8;
inline constexpr double Boltzmann = 1.380649e-23;
inline constexpr double ProtonMassKg = 1.67262192e-27;
inline constexpr double EarthSurfaceGravity = 9.80665;
inline constexpr double EarthSurfacePressurePa = 101325.0;

// Equilibrium temperature of a zero-albedo fast rotator at 1 AU from the Sun.
inline constexpr double EquilibriumTemperatureAt1AuK = 278.6;

} // namespace world::generation::astro
