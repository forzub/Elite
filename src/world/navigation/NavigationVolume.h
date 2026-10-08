#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::navigation
{

// ============================================================================
// PROTECTED NAVIGATION VOLUME CONTRACT
// ============================================================================
//
// NavigationVolume describes PHYSICAL SPACE ONLY.
// It does not mean red/blue/green by itself.
//
// Policy is applied separately:
//   KeepOutside  -> forbidden/red
//   KeepInside   -> mandatory/blue
//   PreferInside -> preferred/green
//
// A swept corridor is an ordered sequence of cross-sections. Straight
// cylinders, bent tunnels, canyons and cave passages are therefore the same
// geometric concept at different section counts.
//
// DO NOT infer this geometry from render meshes. Author it semantically.
// ============================================================================

enum class NavigationVolumeKind : std::uint8_t
{
    Cylinder = 0,
    Box,
    SweptCorridor
};

enum class NavigationCrossSectionKind : std::uint8_t
{
    Circle = 0,
    Rectangle
};

enum class NavigationVolumePolicy : std::uint8_t
{
    None = 0,
    KeepOutside,   // RED: hard exclusion
    KeepInside,    // BLUE: hard containment
    PreferInside   // GREEN: soft routing preference
};

struct NavigationVolumeSection
{
    glm::dvec3 centerLocalMeters {0.0};
    glm::dvec3 forwardLocal {0.0, 0.0, 1.0};
    glm::dvec3 upLocal {0.0, 1.0, 0.0};

    NavigationCrossSectionKind crossSection =
        NavigationCrossSectionKind::Circle;

    double radiusMeters = 0.0;
    double halfWidthMeters = 0.0;
    double halfHeightMeters = 0.0;

    bool finite() const noexcept
    {
        const auto finite3 = [](const glm::dvec3& v)
        {
            return std::isfinite(v.x) &&
                std::isfinite(v.y) &&
                std::isfinite(v.z);
        };

        if (!finite3(centerLocalMeters) ||
            !finite3(forwardLocal) ||
            !finite3(upLocal) ||
            glm::length(forwardLocal) <= 1.0e-9 ||
            glm::length(upLocal) <= 1.0e-9)
        {
            return false;
        }

        if (crossSection == NavigationCrossSectionKind::Circle)
            return std::isfinite(radiusMeters) && radiusMeters > 0.0;

        return
            std::isfinite(halfWidthMeters) &&
            std::isfinite(halfHeightMeters) &&
            halfWidthMeters > 0.0 &&
            halfHeightMeters > 0.0;
    }
};

struct NavigationVolumeDefinition
{
    std::string id;

    // Semantic owner/reference frame. For a hub module this is its module id;
    // for terrain/caves it can be a world-region or other kinematic frame id.
    std::string referenceFrameId;

    NavigationVolumeKind kind = NavigationVolumeKind::SweptCorridor;

    // Primitive representation.
    glm::dvec3 centerLocalMeters {0.0};
    glm::dmat3 localBasis {1.0};
    double radiusMeters = 0.0;
    double halfLengthMeters = 0.0;
    glm::dvec3 halfExtentsMeters {0.0};

    // General representation. First/last sections are normally associated
    // with public navigation portals; interior sections shape bends and
    // variable-width passages but need not be graph nodes.
    std::vector<NavigationVolumeSection> sections;

    // Authored infrastructure clearance in addition to hull envelope.
    double requiredClearanceMeters = 0.0;

    bool finite() const noexcept
    {
        if (id.empty() ||
            referenceFrameId.empty() ||
            !std::isfinite(requiredClearanceMeters) ||
            requiredClearanceMeters < 0.0)
        {
            return false;
        }

        if (kind == NavigationVolumeKind::Cylinder)
        {
            return
                std::isfinite(radiusMeters) &&
                std::isfinite(halfLengthMeters) &&
                radiusMeters > 0.0 &&
                halfLengthMeters > 0.0;
        }

        if (kind == NavigationVolumeKind::Box)
        {
            return
                std::isfinite(halfExtentsMeters.x) &&
                std::isfinite(halfExtentsMeters.y) &&
                std::isfinite(halfExtentsMeters.z) &&
                halfExtentsMeters.x > 0.0 &&
                halfExtentsMeters.y > 0.0 &&
                halfExtentsMeters.z > 0.0;
        }

        if (sections.size() < 2)
            return false;

        for (const auto& section : sections)
        {
            if (!section.finite())
                return false;
        }

        return true;
    }
};

struct NavigationVolumeConstraint
{
    std::string volumeId;
    NavigationVolumePolicy policy = NavigationVolumePolicy::None;

    // Optional route-specific speed ceiling while this policy is active.
    // Zero means no additional speed restriction.
    double maxSpeedMps = 0.0;
};

} // namespace world::navigation
