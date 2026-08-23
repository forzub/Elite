#pragma once

#include <string>

#include "src/game/navigation/GalacticReferenceFrame.h"

namespace game::presentation
{

struct GalacticCompassVocabulary
{
    std::string galacticCenter = "GC";
    std::string galacticAnticenter = "GAC";
    std::string longitude90 = "L90";
    std::string longitude270 = "L270";
    std::string northGalacticPole = "NGP";
    std::string southGalacticPole = "SGP";
};

struct GalacticCompassPresentation
{
    bool visible = false;
    double longitudeDeg = 0.0;
    double latitudeDeg = 0.0;
    GalacticCompassVocabulary vocabulary;
};

inline GalacticCompassPresentation buildGalacticCompassPresentation(
    const game::navigation::GalacticReferenceFrame& frame,
    const glm::dvec3& shipForwardWorld,
    bool visible,
    const GalacticCompassVocabulary& vocabulary = {}
)
{
    GalacticCompassPresentation out;
    out.visible = visible && frame.valid;
    out.vocabulary = vocabulary;
    if (!out.visible)
        return out;

    const auto angles = game::navigation::galacticAnglesForDirection(
        frame,
        shipForwardWorld
    );
    out.longitudeDeg = angles.longitudeDeg;
    out.latitudeDeg = angles.latitudeDeg;
    return out;
}

} // namespace game::presentation
