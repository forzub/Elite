#include "src/game/navigation/NavigationVolumeCatalog.h"

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::navigation
{
namespace
{
using json = nlohmann::json;

world::navigation::NavigationVolumeKind parseVolumeKind(
    const std::string& value
)
{
    if (value == "cylinder")
        return world::navigation::NavigationVolumeKind::Cylinder;
    if (value == "box")
        return world::navigation::NavigationVolumeKind::Box;
    return world::navigation::NavigationVolumeKind::SweptCorridor;
}

world::navigation::NavigationCrossSectionKind parseSectionKind(
    const std::string& value
)
{
    return value == "rectangle"
        ? world::navigation::NavigationCrossSectionKind::Rectangle
        : world::navigation::NavigationCrossSectionKind::Circle;
}

glm::dvec3 readVec3(
    const json& parent,
    const char* key,
    const glm::dvec3& fallback
)
{
    if (!parent.contains(key) ||
        !parent[key].is_array() ||
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

bool NavigationVolumeCatalog::load(const std::string& path)
{
    m_byId.clear();
    m_failure.clear();

    std::ifstream in(path);
    if (!in.is_open() &&
        path == "assets/data/navigation/hub_navigation_volumes.json")
    {
        in.open("../assets/data/navigation/hub_navigation_volumes.json");
    }
    if (!in.is_open() &&
        path == "assets/data/navigation/hub_navigation_volumes.json")
    {
        in.open("src/assets/data/navigation/hub_navigation_volumes.json");
    }

    if (!in.is_open())
    {
        m_failure = "cannot open navigation volume catalog";
        return false;
    }

    json root;
    try
    {
        in >> root;
    }
    catch (const std::exception& error)
    {
        m_failure =
            std::string("navigation volume JSON parse failed: ") +
            error.what();
        return false;
    }

    if (root.value("version", 0u) != 1u ||
        !root.contains("volumes") ||
        !root["volumes"].is_array())
    {
        m_failure = "invalid navigation volume catalog root";
        return false;
    }

    for (const auto& item : root["volumes"])
    {
        if (!item.is_object())
            continue;

        world::navigation::NavigationVolumeDefinition volume;
        volume.id = item.value("id", "");
        volume.referenceFrameId =
            item.value("reference_frame_id", "");
        volume.kind =
            parseVolumeKind(item.value("kind", "swept_corridor"));
        volume.centerLocalMeters =
            readVec3(item, "center_local_m", glm::dvec3(0.0));
        volume.radiusMeters = item.value("radius_m", 0.0);
        volume.halfLengthMeters = item.value("half_length_m", 0.0);
        volume.halfExtentsMeters =
            readVec3(item, "half_extents_m", glm::dvec3(0.0));
        volume.requiredClearanceMeters =
            item.value("required_clearance_m", 0.0);

        if (item.contains("sections") &&
            item["sections"].is_array())
        {
            for (const auto& sectionItem : item["sections"])
            {
                if (!sectionItem.is_object())
                    continue;

                world::navigation::NavigationVolumeSection section;
                section.centerLocalMeters =
                    readVec3(
                        sectionItem,
                        "center_local_m",
                        glm::dvec3(0.0)
                    );
                section.forwardLocal =
                    readVec3(
                        sectionItem,
                        "forward_local",
                        glm::dvec3(0.0, 0.0, 1.0)
                    );
                section.upLocal =
                    readVec3(
                        sectionItem,
                        "up_local",
                        glm::dvec3(0.0, 1.0, 0.0)
                    );
                section.crossSection =
                    parseSectionKind(
                        sectionItem.value(
                            "cross_section",
                            "circle"
                        )
                    );
                section.radiusMeters =
                    sectionItem.value("radius_m", 0.0);
                section.halfWidthMeters =
                    sectionItem.value("half_width_m", 0.0);
                section.halfHeightMeters =
                    sectionItem.value("half_height_m", 0.0);
                volume.sections.push_back(std::move(section));
            }
        }

        if (!volume.finite())
        {
            m_failure =
                "invalid navigation volume definition: " + volume.id;
            m_byId.clear();
            return false;
        }

        if (!m_byId.emplace(volume.id, std::move(volume)).second)
        {
            m_failure = "duplicate navigation volume id";
            m_byId.clear();
            return false;
        }
    }

    return true;
}

const world::navigation::NavigationVolumeDefinition*
NavigationVolumeCatalog::find(
    const std::string& id
) const noexcept
{
    const auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &it->second;
}

} // namespace game::navigation
