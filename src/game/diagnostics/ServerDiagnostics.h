#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace game::diagnostics
{

struct ServerDiagnosticsSettings
{
    // Disabled by default. CSV capture is opt-in and must never be part of the
    // normal simulation hot path unless explicitly enabled for diagnostics.
    bool playerMotionCsv = false;
    bool gravityCsv = false;
    bool serverNavigationCsv = false;
    bool hubPlayerChainCsv = false;
};

struct SimulationDiagnosticsState
{
    std::uint32_t gravityRows = 0;
    std::uint32_t serverNavigationRows = 0;
    std::uint32_t playerMotionRows = 0;
    std::uint32_t hubPlayerChainRows = 0;

    bool serverNavigationHubOrbitInitialized = false;
    glm::dvec3 serverNavigationHubStartMeters {0.0};
    glm::dvec3 serverNavigationHubStartRadial {1.0, 0.0, 0.0};
    double serverNavigationHubStartAngleDeg = 0.0;

    bool hubPlayerChainHasPreviousSample = false;
    glm::dvec3 hubPlayerChainPreviousHubMeters {0.0};
    glm::dvec3 hubPlayerChainPreviousPlayerLocalMeters {0.0};
    glm::dvec3 hubPlayerChainPreviousStationLocalMeters {0.0};
};

struct GameServerDiagnosticsState
{
    std::uint32_t hubPlayerRoundTripWarnings = 0;
    std::uint32_t hubGeometryWarnings = 0;
};

struct ServerDiagnostics
{
    ServerDiagnosticsSettings settings;
    SimulationDiagnosticsState simulation;
    GameServerDiagnosticsState server;

    void resetCaptureState()
    {
        simulation = SimulationDiagnosticsState{};
        server = GameServerDiagnosticsState{};
    }
};

} // namespace game::diagnostics
