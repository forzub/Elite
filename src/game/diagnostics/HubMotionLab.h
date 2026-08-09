#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

namespace game::diagnostics
{

/*
    Debug-only motion laboratory used to compare client presentation paths.

    The three NPC actors are authoritative server samples that travel through
    the normal ShipSnapshot -> ClientWorldState interpolation path. The cube is
    intentionally different: it is a presentation-only analytic reference that
    is evaluated directly from synchronized server time on the client.
*/
inline constexpr bool HubMotionLabEnabled = true;
inline constexpr const char* HubMotionLabHubId = "earth_orbital_hub";
inline constexpr int HubMotionLabSystemId = 0;

enum class HubMotionLabActorKind : std::uint8_t
{
    None = 0,
    SlowOrbit,
    FastOrbit,
    MatchPlayer
};

struct HubMotionLabLocalState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMetersPerSecond {0.0};
};

struct HubMotionLabActorSpec
{
    HubMotionLabActorKind kind = HubMotionLabActorKind::None;
    const char* label = "LAB";
    double radiusMeters = 0.0;
    double radialOffsetMeters = 0.0;
    double tangentialSpeedMetersPerSecond = 0.0;
    double phaseRadians = 0.0;
};

inline constexpr std::array<HubMotionLabActorSpec, 3> HubMotionLabActors {{
    {
        HubMotionLabActorKind::SlowOrbit,
        "LAB NPC SLOW",
        9500.0,
        2400.0,
        45.0,
        4.70
    },
    {
        HubMotionLabActorKind::FastOrbit,
        "LAB NPC FAST",
        10500.0,
        2600.0,
        180.0,
        4.75
    },
    {
        HubMotionLabActorKind::MatchPlayer,
        "LAB NPC MATCH",
        150.0,
        80.0,
        1.0,
        1.20
    }
}};

inline constexpr const HubMotionLabActorSpec* hubMotionLabSpec(
    HubMotionLabActorKind kind
) noexcept
{
    for (const auto& spec : HubMotionLabActors)
    {
        if (spec.kind == kind)
            return &spec;
    }

    return nullptr;
}

inline constexpr const char* hubMotionLabLabel(
    HubMotionLabActorKind kind
) noexcept
{
    const auto* spec = hubMotionLabSpec(kind);
    return spec ? spec->label : "";
}

inline HubMotionLabLocalState evaluateHubMotionLabActor(
    HubMotionLabActorKind kind,
    double serverTimeSeconds,
    const glm::dvec3& playerLocalPositionMeters = glm::dvec3(0.0),
    const glm::dvec3& playerLocalVelocityMetersPerSecond = glm::dvec3(0.0)
)
{
    HubMotionLabLocalState state;

    const auto* spec = hubMotionLabSpec(kind);
    if (!spec)
        return state;

    if (kind == HubMotionLabActorKind::MatchPlayer)
    {
        // Keep one remote actor almost co-moving with the authoritative
        // player. Its relative motion is only 1 m/s, so epoch/reference-frame
        // mistakes become immediately visible as jitter near the camera.
        const double omega =
            spec->radiusMeters > 0.0
                ? spec->tangentialSpeedMetersPerSecond / spec->radiusMeters
                : 0.0;

        const double a = spec->phaseRadians + omega * serverTimeSeconds;
        const double c = std::cos(a);
        const double s = std::sin(a);

        const glm::dvec3 relativePosition {
            spec->radiusMeters * c,
            spec->radialOffsetMeters,
            spec->radiusMeters * s
        };

        const glm::dvec3 relativeVelocity {
            -spec->radiusMeters * omega * s,
            0.0,
            spec->radiusMeters * omega * c
        };

        state.positionMeters =
            playerLocalPositionMeters + relativePosition;
        state.velocityMetersPerSecond =
            playerLocalVelocityMetersPerSecond + relativeVelocity;
        return state;
    }

    const double omega =
        spec->radiusMeters > 0.0
            ? spec->tangentialSpeedMetersPerSecond / spec->radiusMeters
            : 0.0;

    const double a = spec->phaseRadians + omega * serverTimeSeconds;
    const double c = std::cos(a);
    const double s = std::sin(a);

    state.positionMeters = {
        spec->radiusMeters * c,
        spec->radialOffsetMeters,
        spec->radiusMeters * s
    };

    state.velocityMetersPerSecond = {
        -spec->radiusMeters * omega * s,
        0.0,
        spec->radiusMeters * omega * c
    };

    return state;
}

struct HubMotionLabCubePose
{
    glm::dvec3 localPositionMeters {0.0};
    double localRotationRadians = 0.0;
    double halfExtentMeters = 100.0;
};

inline HubMotionLabCubePose evaluateHubMotionLabCube(
    double presentationServerTimeSeconds
)
{
    constexpr double radiusMeters = 450.0;
    constexpr double radialOffsetMeters = 300.0;
    constexpr double tangentialSpeedMetersPerSecond = 12.0;
    constexpr double phaseRadians = 0.65;
    constexpr double selfRotationRadiansPerSecond = 0.70;

    const double omega =
        tangentialSpeedMetersPerSecond / radiusMeters;

    const double a =
        phaseRadians + omega * presentationServerTimeSeconds;

    HubMotionLabCubePose pose;
    pose.localPositionMeters = {
        radiusMeters * std::cos(a),
        radialOffsetMeters,
        radiusMeters * std::sin(a)
    };
    pose.localRotationRadians =
        selfRotationRadiansPerSecond * presentationServerTimeSeconds;
    pose.halfExtentMeters = 100.0;
    return pose;
}

} // namespace game::diagnostics
