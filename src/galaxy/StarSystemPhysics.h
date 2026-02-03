#pragma once
#include <string>
#include <vector>
#include <glm/vec3.hpp>

struct StarData
{
    std::string spectralClass;
    float mass;
    float luminosity;
};

struct PlanetData
{
    std::string name;
    float orbitRadius;
    bool habitable;
    bool inhabited;
    NodeId node; // если планета заселена и имеет Node
};

struct AsteroidBeltData
{
    float innerRadius;
    float outerRadius;
    std::string composition;
};


struct StarSystemPhysics
{
    StarSystemId systemId;

    // Звезда (может отсутствовать)
    std::optional<StarData> star;

    // Планетарная система
    std::vector<PlanetData> planets;

    // Пояса
    std::vector<AsteroidBeltData> belts;

    // Особенности
    std::vector<std::string> anomalies;
};
