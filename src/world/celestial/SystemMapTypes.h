#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/CelestialTypes.h"

namespace world::celestial
{
    struct GalaxyMapSystem
    {
        int id = -1;

        std::string name;
        std::string starType;

        int starsCount = 1;

        glm::dvec3 positionLy {0.0};
        std::string jurisdiction;
    };

    struct GalaxyMapSnapshot
    {
        double universeTimeSeconds = 0.0;
        std::string universeDate;

        std::vector<GalaxyMapSystem> systems;
    };

    struct SystemMapRing
    {
        std::string name;

        double innerRadiusKm = 0.0;
        double outerRadiusKm = 0.0;

        std::string composition;
    };

    struct SystemMapBody
    {
        std::string id;
        std::string name;
        std::string parentId;

        BodyType type = BodyType::Unknown;

        glm::dvec3 positionAu {0.0};

        // Готовые данные для рендера орбиты.
        // Рендерер больше не должен угадывать центр орбиты по parentId.
        glm::dvec3 orbitCenterAu {0.0};
        double orbitRadiusAu = 0.0;
        bool drawOrbit = false;

        double radiusKm = 0.0;

        // Пока цвет fallback-овый.
        // Потом можно брать из system_details.json, если поле цвета есть.
        glm::vec4 color {0.6f, 0.8f, 1.0f, 1.0f};

        std::vector<SystemMapRing> rings;
    };

    struct SystemMapSnapshot
    {
        int systemId = -1;
        std::string systemName;

        double universeTimeSeconds = 0.0;
        std::string universeDate;

        std::vector<SystemMapBody> bodies;
    };
}