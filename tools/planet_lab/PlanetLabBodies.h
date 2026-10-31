#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/CloudWeatherField.h"
#include "src/world/celestial/surface/SurfaceField.h"

namespace planet_lab
{
using namespace world::celestial::surface;

/*
    The Lab's bodies, all from the repository's generated physical
    descriptors (procedural provider):

      earth_like  Sol / Earth descriptor (plate tectonics, 70 % ocean,
                  23.4 deg obliquity) - the game keeps its imported real
                  maps; here the same physics runs through the procedural
                  provider;
      rocky       Sol / Mercury (airless, cratered highlands);
      asteroid    a body of the Sol asteroid belt reservoir, derived from
                  (belt seed, cell, candidate);
      gas_giant   Sol / Jupiter;
      dry         Sol / Mars (thin CO2, dune desert, polar caps);
      hostile     Alpha Centauri A i (180 bar: ground unreachable).
*/
struct LabBody
{
    std::string label;
    std::string sourcePath;
    BodySurfaceDescriptor descriptor;          // planets / moons
    SmallBodyDescriptor smallBody;             // asteroid
    bool isSmallBody = false;
    std::shared_ptr<const SurfaceField> field;
    std::shared_ptr<const CloudWeatherField> clouds; // null: no atmosphere layers
    SurfaceDetailPolicy policy;
    glm::dvec3 focusDir {0.0, 0.0, 1.0};       // the viewpoints' surface point
    std::string focusLabel;
};

// root: the repository root (data under src/assets/data/galaxy_details).
bool loadLabBodies(const std::string& root, std::vector<LabBody>& out, std::string& error);

// A body from any generated body JSON (--body).
bool loadLabBodyFromFile(const std::string& path, LabBody& out, std::string& error);
} // namespace planet_lab
