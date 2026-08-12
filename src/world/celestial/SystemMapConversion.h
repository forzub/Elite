#pragma once

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace world::celestial
{

inline SystemMapRingDisplayMode toSystemMapRingDisplayMode(
    CelestialRingDisplayMode mode
)
{
    return mode == CelestialRingDisplayMode::ParticleCloud
        ? SystemMapRingDisplayMode::ParticleCloud
        : SystemMapRingDisplayMode::LayeredBands;
}

inline SystemMapRingVisibilityClass toSystemMapRingVisibilityClass(
    CelestialRingVisibilityClass value
)
{
    switch (value)
    {
        case CelestialRingVisibilityClass::Main:
            return SystemMapRingVisibilityClass::Main;
        case CelestialRingVisibilityClass::Secondary:
            return SystemMapRingVisibilityClass::Secondary;
        case CelestialRingVisibilityClass::Diffuse:
            return SystemMapRingVisibilityClass::Diffuse;
        case CelestialRingVisibilityClass::Faint:
        default:
            return SystemMapRingVisibilityClass::Faint;
    }
}

inline SystemMapRingVisualProfile toSystemMapRingVisualProfile(
    const CelestialRingSystemVisualProfile& source
)
{
    SystemMapRingVisualProfile result;

    result.displayProfile = source.displayProfile;
    result.renderMode = toSystemMapRingDisplayMode(source.renderMode);
    result.recognizabilityPriority = source.recognizabilityPriority;
    result.artisticWidthScale = source.artisticWidthScale;
    result.mainBandEmphasis = source.mainBandEmphasis;
    result.secondaryBandEmphasis = source.secondaryBandEmphasis;
    result.faintBandEmphasis = source.faintBandEmphasis;
    result.diffuseBandEmphasis = source.diffuseBandEmphasis;
    result.gapContrast = source.gapContrast;
    result.radialStructureStrength = source.radialStructureStrength;
    result.fineStructureStrength = source.fineStructureStrength;
    result.edgeSoftnessScale = source.edgeSoftnessScale;
    result.brightnessVariation = source.brightnessVariation;
    result.minimumVisibleWidthPx = source.minimumVisibleWidthPx;
    result.minimumMainBandWidthPx = source.minimumMainBandWidthPx;
    result.continuousFill = source.continuousFill;
    result.particleDensity = source.particleDensity;
    result.particleOpacityScale = source.particleOpacityScale;
    result.particleSizePxRange = source.particleSizePxRange;
    result.radialJitter = source.radialJitter;
    result.azimuthalClumping = source.azimuthalClumping;
    result.clusterScale = source.clusterScale;
    result.softness = source.softness;
    result.artisticOcclusionEnabled = source.artisticOcclusionEnabled;
    result.backHalfBrightness = source.backHalfBrightness;
    result.innerEdgeDarkening = source.innerEdgeDarkening;

    return result;
}

inline SystemMapRing toSystemMapRing(
    const CelestialRingDefinition& source
)
{
    SystemMapRing result;

    result.name = source.name;
    result.innerRadiusKm = source.innerRadiusKm;
    result.outerRadiusKm = source.outerRadiusKm;
    result.composition = source.composition;
    result.tint = source.render.tint;
    result.opacity = source.render.opacity;
    result.opticalDepth = source.render.opticalDepth;
    result.radialNoiseStrength = source.render.radialNoiseStrength;
    result.radialBrightnessVariation = source.render.radialBrightnessVariation;
    result.azimuthalAsymmetry = source.render.azimuthalAsymmetry;
    result.edgeSoftness = source.render.edgeSoftness;
    result.visibilityClass =
        toSystemMapRingVisibilityClass(source.render.visibilityClass);
    result.displayMode =
        toSystemMapRingDisplayMode(source.render.displayMode);
    result.visualOpacityScale = source.render.visualOpacityScale;
    result.radialStructureScale = source.render.radialStructureScale;
    result.particleDensityScale = source.render.particleDensityScale;
    result.particleClumpiness = source.render.particleClumpiness;
    result.particleRadialJitter = source.render.particleRadialJitter;
    result.particleSizePxRange = source.render.particleSizePxRange;
    result.castsShadow = source.render.castsShadow;

    return result;
}

} // namespace world::celestial
