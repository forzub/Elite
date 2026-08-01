#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/game/navigation/GalaxyNavigationConfig.h"

namespace game::navigation
{

/*
    Galaxy and System navigation use the same address and cell contracts.
    Galaxy keeps unit-specific convenience method names only at its boundary.
*/
using GalaxyNavigationFrame = CubicNavigationFrame;
using GalaxyGridIndex = CubicGridIndex;
using GalaxyNavigationCell = CubicNavigationCell;

class GalaxyNavigationDomainPolicy;

/*
    Galaxy specialization of the shared cubic navigation core.

    The base owns hierarchy, anchor, hover and explicit selection. This class
    contributes only Galaxy-specific configuration, light-year naming, display
    radius and the navigable root-domain whitelist.
*/
class GalaxyNavigationGrid final : public CubicNavigationGrid
{
public:
    GalaxyNavigationGrid();

    void reset() override;

    const GalaxyNavigationConfig& config() const;

    void synchronizeCatalogPositions(
        const std::vector<glm::dvec3>& positionsLy
    );

    const std::vector<std::array<std::int64_t, 3>>&
    allowedRootCells() const;

    double cellSizeLy() const;
    double cellSizeLy(int level) const;

    int displayRadius() const;
    void setDisplayRadius(int radius);

    void setAnchorFromPositionLy(const glm::dvec3& positionLy);

    glm::dvec3 cellCenterLy(
        const GalaxyGridIndex& index,
        int level
    ) const;

    GalaxyGridIndex nearestIndexForPositionLy(
        const glm::dvec3& positionLy,
        int level
    ) const;

    GalaxyGridIndex rootIndexForCell(
        const GalaxyGridIndex& index,
        int level
    ) const;

    std::vector<GalaxyNavigationCell> neighborhood() const;

private:
    explicit GalaxyNavigationGrid(GalaxyNavigationConfig config);

    static GalaxyNavigationConfig loadConfig();

    static CubicNavigationGridDefinition makeDefinition(
        const GalaxyNavigationConfig& config
    );

    std::array<std::int64_t, 3> rootIndexForPositionLy(
        const glm::dvec3& positionLy
    ) const;

private:
    GalaxyNavigationConfig m_config;
    std::shared_ptr<GalaxyNavigationDomainPolicy> m_domainPolicy;
    int m_displayRadius = 1;
};

} // namespace game::navigation
