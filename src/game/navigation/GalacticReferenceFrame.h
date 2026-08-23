#pragma once

#include <string>

#include <glm/glm.hpp>

namespace game::navigation
{

struct GalacticReferenceFrame
{
    // Unit world directions for standard galactic coordinates.
    glm::dvec3 centerDir {1.0, 0.0, 0.0};   // l=0, b=0
    glm::dvec3 longitude90Dir {0.0, 1.0, 0.0}; // l=90, b=0
    glm::dvec3 northDir {0.0, 0.0, 1.0};    // b=+90
    bool valid = false;
};

GalacticReferenceFrame makeGalacticReferenceFrame(
    const glm::dvec3& centerDir,
    const glm::dvec3& northDir
);

bool loadGalacticReferenceFrame(
    GalacticReferenceFrame& out,
    const std::string& path = "assets/data/galaxy/milky_way.json"
);

struct GalacticAngles
{
    double longitudeDeg = 0.0; // standard galactic longitude l: [0,360)
    double latitudeDeg = 0.0;  // standard galactic latitude b: [-90,+90]
};

GalacticAngles galacticAnglesForDirection(
    const GalacticReferenceFrame& frame,
    const glm::dvec3& worldDirection
);

glm::dvec3 galacticDirectionForAngles(
    const GalacticReferenceFrame& frame,
    double longitudeDeg,
    double latitudeDeg
);

} // namespace game::navigation
