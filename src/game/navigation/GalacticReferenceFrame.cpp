#include "src/game/navigation/GalacticReferenceFrame.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

namespace game::navigation
{
namespace
{
constexpr double Pi = 3.14159265358979323846;

bool finiteVec(const glm::dvec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

glm::dvec3 readVec3(
    const nlohmann::json& parent,
    const char* key,
    const glm::dvec3& fallback
)
{
    if (!parent.contains(key) || !parent[key].is_array() ||
        parent[key].size() != 3)
    {
        return fallback;
    }

    return {
        parent[key][0].get<double>(),
        parent[key][1].get<double>(),
        parent[key][2].get<double>()
    };
}

} // namespace

GalacticReferenceFrame makeGalacticReferenceFrame(
    const glm::dvec3& centerDir,
    const glm::dvec3& northDir
)
{
    GalacticReferenceFrame out;
    if (!finiteVec(centerDir) || !finiteVec(northDir) ||
        glm::length(centerDir) <= 1.0e-9 ||
        glm::length(northDir) <= 1.0e-9)
    {
        return out;
    }

    out.centerDir = glm::normalize(centerDir);
    glm::dvec3 north = northDir -
        out.centerDir * glm::dot(northDir, out.centerDir);
    if (glm::length(north) <= 1.0e-9)
        return out;

    out.northDir = glm::normalize(north);
    out.longitude90Dir = glm::cross(out.northDir, out.centerDir);
    if (glm::length(out.longitude90Dir) <= 1.0e-9)
        return out;
    out.longitude90Dir = glm::normalize(out.longitude90Dir);
    out.valid = true;
    return out;
}

bool loadGalacticReferenceFrame(
    GalacticReferenceFrame& out,
    const std::string& path
)
{
    std::ifstream in(path);
    if (!in.is_open() && path == "assets/data/galaxy/milky_way.json")
        in.open("../assets/data/galaxy/milky_way.json");
    if (!in.is_open())
        return false;

    nlohmann::json root;
    try
    {
        in >> root;
    }
    catch (...)
    {
        return false;
    }

    if (!root.contains("orientation") || !root["orientation"].is_object())
        return false;

    const auto& o = root["orientation"];
    const glm::dvec3 center = readVec3(
        o,
        "galactic_center_dir",
        glm::dvec3(-0.0549, -0.8734, -0.4838)
    );
    const glm::dvec3 north = readVec3(
        o,
        "galactic_north_dir",
        glm::dvec3(-0.8676, -0.1981, 0.4559)
    );

    out = makeGalacticReferenceFrame(center, north);
    return out.valid;
}

GalacticAngles galacticAnglesForDirection(
    const GalacticReferenceFrame& frame,
    const glm::dvec3& worldDirection
)
{
    GalacticAngles out;
    if (!frame.valid || glm::length(worldDirection) <= 1.0e-12)
        return out;

    const glm::dvec3 d = glm::normalize(worldDirection);
    const double x = glm::dot(d, frame.centerDir);
    const double y = glm::dot(d, frame.longitude90Dir);
    const double z = std::clamp(glm::dot(d, frame.northDir), -1.0, 1.0);

    double longitude = std::atan2(y, x) * 180.0 / Pi;
    if (longitude < 0.0)
        longitude += 360.0;

    out.longitudeDeg = longitude;
    out.latitudeDeg = std::asin(z) * 180.0 / Pi;
    return out;
}

glm::dvec3 galacticDirectionForAngles(
    const GalacticReferenceFrame& frame,
    double longitudeDeg,
    double latitudeDeg
)
{
    if (!frame.valid)
        return glm::dvec3(0.0);

    const double l = longitudeDeg * Pi / 180.0;
    const double b = latitudeDeg * Pi / 180.0;
    const double cb = std::cos(b);

    return glm::normalize(
        frame.centerDir * (cb * std::cos(l)) +
        frame.longitude90Dir * (cb * std::sin(l)) +
        frame.northDir * std::sin(b)
    );
}

} // namespace game::navigation
