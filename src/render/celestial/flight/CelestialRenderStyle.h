#pragma once

#include <string>

namespace render::celestial_flight
{
/*
    Presentation style of celestial bodies (stars, planets, moons, gas
    giants, clouds, atmospheres, rings, asteroids). One renderer, one world
    model, three shading paths:
      EliteStyle  monochrome vector look: opaque black bodies, coastlines,
                  height contours, lattice wireframe of the local terrain,
                  rim / ring / star outlines;
      Modern      lit albedo + relief normals, water specular, night lights,
                  scattering atmosphere, aerial haze, volumetric-looking
                  clouds;
      Anime       cel shading: posterised palette, hard light bands with
                  hue-shifted shadows, ink outlines, flat clouds, stylised
                  sky and star flares.
    User setting: ClientModeState::celestialRenderStyleId ("elite" /
    "modern" / "anime"), persisted in ClientPreferences, cycled with
    Ctrl+F7 in flight and chosen in Main menu -> Settings.
*/
enum class CelestialRenderStyle
{
    EliteStyle = 0,
    Modern = 1,
    Anime = 2
};

inline const char* celestialRenderStyleId(CelestialRenderStyle style)
{
    switch (style)
    {
        case CelestialRenderStyle::EliteStyle: return "elite";
        case CelestialRenderStyle::Anime: return "anime";
        default: return "modern";
    }
}

inline bool parseCelestialRenderStyle(const std::string& id, CelestialRenderStyle& out)
{
    if (id == "elite") { out = CelestialRenderStyle::EliteStyle; return true; }
    if (id == "modern") { out = CelestialRenderStyle::Modern; return true; }
    if (id == "anime") { out = CelestialRenderStyle::Anime; return true; }
    return false;
}

inline CelestialRenderStyle nextCelestialRenderStyle(CelestialRenderStyle style)
{
    switch (style)
    {
        case CelestialRenderStyle::EliteStyle: return CelestialRenderStyle::Modern;
        case CelestialRenderStyle::Modern: return CelestialRenderStyle::Anime;
        default: return CelestialRenderStyle::EliteStyle;
    }
}
} // namespace render::celestial_flight
