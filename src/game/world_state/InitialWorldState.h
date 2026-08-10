#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <utility>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/world/orbits/OrbitalMotion.h"

namespace game::world_state
{

struct InitialWorldStateMotion
{
    std::string type = "fixed";
    world::orbits::OrbitalPeriodPolicy orbitalPeriodPolicy =
        world::orbits::OrbitalPeriodPolicy::Fixed;

    double altitudeKm = 0.0;

    double orbitalPeriodSeconds = 1.0;
    double selfRotationPeriodSeconds = 1.0;

    double inclinationDeg = 0.0;
    double longitudeOfAscendingNodeDeg = 0.0;
    double argumentOfPeriapsisDeg = 0.0;
    double initialPhaseDeg = 0.0;

    double epochSeconds = 0.0;
};

struct InitialWorldStateHubModule
{
    std::string id;
    std::string type;
    std::string name;

    bool mapVisible = true;

    glm::dvec3 offsetMeters {0.0};

    // Local placement rotation inside hub coordinates.
    // pitch = rotation around local X
    // yaw   = rotation around local Y
    // roll  = rotation around local Z
    glm::dvec3 localRotationDeg {0.0};

    bool exists = true;
};

struct InitialWorldStateOrbitalHub
{
    std::string id;
    std::string name;
    std::string owner;

    int systemId = -1;
    std::string parentBodyId;
    std::string mapObjectModuleId;

    InitialWorldStateMotion motion;
    std::vector<InitialWorldStateHubModule> modules;
};


struct InitialWorldStatePlayerStart
{
    int systemId = -1;
    std::string hubId;
    glm::dvec3 localOffsetMeters {0.0};

    bool valid() const noexcept
    {
        return systemId >= 0 && !hubId.empty();
    }
};

struct InitialWorldStateSystemState
{
    int systemId = -1;
    std::string jurisdiction = "Unregistered";
};

struct InitialWorldState
{
    int version = 1;

    InitialWorldStatePlayerStart playerStart;
    // Mutable/authored facts keyed by physical StarAtlas system id.
    std::vector<InitialWorldStateSystemState> systemStates;
    std::vector<InitialWorldStateOrbitalHub> orbitalHubs;
};

inline glm::dvec3 readDvec3Meters(
    const nlohmann::json& j
)
{
    return glm::dvec3(
        j.value("x", 0.0),
        j.value("y", 0.0),
        j.value("z", 0.0)
    );
}


inline glm::dvec3 readEulerDeg(
    const nlohmann::json& j
)
{
    return glm::dvec3(
        j.value("pitch", 0.0),
        j.value("yaw",   0.0),
        j.value("roll",  0.0)
    );
}



inline InitialWorldStateMotion readMotion(
    const nlohmann::json& j
)
{
    InitialWorldStateMotion out;

    out.type =
        j.value("type", "fixed");
        
    const std::string periodMode =
        j.value("orbital_period_mode", "fixed");

    if (periodMode == "kepler")
    {
        out.orbitalPeriodPolicy =
            world::orbits::OrbitalPeriodPolicy::Kepler;
    }
    else if (periodMode == "fixed")
    {
        out.orbitalPeriodPolicy =
            world::orbits::OrbitalPeriodPolicy::Fixed;
    }
    else
    {
        throw std::runtime_error(
            "unsupported orbital_period_mode: " + periodMode
        );
    }

    out.altitudeKm =
        j.value("altitude_km", 0.0);

    out.orbitalPeriodSeconds =
        j.value("orbital_period_seconds", 1.0);

    out.selfRotationPeriodSeconds =
        j.value("self_rotation_period_seconds", 1.0);

    out.inclinationDeg =
        j.value("inclination_deg", 0.0);

    out.longitudeOfAscendingNodeDeg =
        j.value("longitude_of_ascending_node_deg", 0.0);

    out.argumentOfPeriapsisDeg =
        j.value("argument_of_periapsis_deg", 0.0);

    out.initialPhaseDeg =
        j.value("initial_phase_deg", 0.0);

    out.epochSeconds =
        j.value("epoch_seconds", 0.0);

    return out;
}

inline InitialWorldStateHubModule readHubModule(
    const nlohmann::json& j
)
{
    InitialWorldStateHubModule out;

    out.id =
        j.value("id", "");

    out.type =
        j.value("type", "");

    out.name =
        j.value("name", out.id);

    out.mapVisible =
        j.value("map_visible", true);

    if (j.contains("offset_m") && j["offset_m"].is_object())
        out.offsetMeters = readDvec3Meters(j["offset_m"]);

    if (j.contains("local_rotation_deg") &&
        j["local_rotation_deg"].is_object())
    {
        out.localRotationDeg =
            readEulerDeg(
                j["local_rotation_deg"]
            );
    }

    if (j.contains("state") && j["state"].is_object())
    {
        const auto& state = j["state"];

        out.exists =
            state.value("exists", true);

    }

    return out;
}

inline InitialWorldStateOrbitalHub readOrbitalHub(
    const nlohmann::json& j
)
{
    InitialWorldStateOrbitalHub out;

    out.id =
        j.value("id", "");

    out.name =
        j.value("name", out.id);

    out.owner =
        j.value("owner", "Independent");

    out.systemId =
        j.value("system_id", -1);

    out.parentBodyId =
        j.value("parent_body_id", "");

    out.mapObjectModuleId =
        j.value("map_object_module_id", "");

    if (j.contains("motion") && j["motion"].is_object())
        out.motion = readMotion(j["motion"]);

    if (j.contains("modules") && j["modules"].is_array())
    {
        for (const auto& moduleJson : j["modules"])
        {
            if (!moduleJson.is_object())
                continue;

            auto module =
                readHubModule(moduleJson);

            if (!module.id.empty())
                out.modules.push_back(std::move(module));
        }
    }

    return out;
}

inline InitialWorldStatePlayerStart readPlayerStart(
    const nlohmann::json& j
)
{
    InitialWorldStatePlayerStart out;

    out.systemId = j.value("system_id", -1);
    out.hubId = j.value("hub_id", "");

    if (j.contains("local_offset_m") &&
        j["local_offset_m"].is_object())
    {
        out.localOffsetMeters =
            readDvec3Meters(j["local_offset_m"]);
    }

    return out;
}

inline InitialWorldStateSystemState readSystemState(
    const nlohmann::json& j
)
{
    InitialWorldStateSystemState out;
    out.systemId = j.value("system_id", -1);
    out.jurisdiction = j.value("jurisdiction", "Unregistered");
    return out;
}

inline bool validateInitialWorldState(
    const InitialWorldState& state,
    std::string& error
)
{
    if (state.version != 1)
    {
        error = "unsupported initial world version: " +
            std::to_string(state.version);
        return false;
    }

    if (!state.playerStart.valid())
    {
        error = "player_start must define system_id and hub_id";
        return false;
    }

    const InitialWorldStateOrbitalHub* playerHub = nullptr;
    for (const auto& hub : state.orbitalHubs)
    {
        if (hub.id == state.playerStart.hubId)
        {
            playerHub = &hub;
            break;
        }
    }

    if (!playerHub)
    {
        error = "player_start references unknown hub: " +
            state.playerStart.hubId;
        return false;
    }

    if (playerHub->systemId != state.playerStart.systemId)
    {
        error = "player_start system_id does not match its hub";
        return false;
    }

    for (std::size_t i = 0; i < state.systemStates.size(); ++i)
    {
        const auto& systemState = state.systemStates[i];
        if (systemState.systemId < 0)
        {
            error = "system state has invalid system_id";
            return false;
        }

        for (std::size_t j = i + 1; j < state.systemStates.size(); ++j)
        {
            if (systemState.systemId == state.systemStates[j].systemId)
            {
                error = "duplicate system state for system_id=" +
                    std::to_string(systemState.systemId);
                return false;
            }
        }
    }

    for (std::size_t i = 0; i < state.orbitalHubs.size(); ++i)
    {
        const auto& hub = state.orbitalHubs[i];
        if (hub.id.empty() || hub.systemId < 0 || hub.parentBodyId.empty())
        {
            error = "orbital hub has incomplete identity/reference data";
            return false;
        }

        if (hub.motion.type != "parent_orbit")
        {
            error = "unsupported orbital hub motion type: " + hub.motion.type;
            return false;
        }

        if (hub.motion.orbitalPeriodPolicy ==
                world::orbits::OrbitalPeriodPolicy::Fixed &&
            hub.motion.orbitalPeriodSeconds <= 0.0)
        {
            error = "fixed orbital period must be positive for hub: " + hub.id;
            return false;
        }

        if (hub.motion.selfRotationPeriodSeconds <= 0.0)
        {
            error = "self rotation period must be positive for hub: " + hub.id;
            return false;
        }

        for (std::size_t j = i + 1; j < state.orbitalHubs.size(); ++j)
        {
            if (hub.id == state.orbitalHubs[j].id)
            {
                error = "duplicate orbital hub id: " + hub.id;
                return false;
            }
        }

        for (std::size_t moduleIndex = 0;
             moduleIndex < hub.modules.size();
             ++moduleIndex)
        {
            if (hub.modules[moduleIndex].type != "command_station")
            {
                error = "unsupported hub module type: " +
                    hub.modules[moduleIndex].type;
                return false;
            }

            for (std::size_t other = moduleIndex + 1;
                 other < hub.modules.size();
                 ++other)
            {
                if (hub.modules[moduleIndex].id == hub.modules[other].id)
                {
                    error = "duplicate hub module id: " +
                        hub.modules[moduleIndex].id;
                    return false;
                }
            }
        }

        if (!hub.mapObjectModuleId.empty())
        {
            const InitialWorldStateHubModule* mapModule = nullptr;
            for (const auto& module : hub.modules)
            {
                if (module.id == hub.mapObjectModuleId)
                {
                    mapModule = &module;
                    break;
                }
            }
            if (!mapModule)
            {
                error = "hub map_object_module_id is not a declared module: " +
                    hub.id;
                return false;
            }

            if (hub.id == state.playerStart.hubId && !mapModule->exists)
            {
                error = "player_start hub map representative does not exist";
                return false;
            }

            if (hub.id == state.playerStart.hubId && !mapModule->mapVisible)
            {
                error = "player_start hub map representative is hidden";
                return false;
            }
        }
    }

    return true;
}

inline bool loadInitialWorldState(
    const std::string& path,
    InitialWorldState& out
)
{
    std::ifstream f(path);

    if (!f.is_open())
        return false;

    try
    {
        nlohmann::json root;
        f >> root;

        InitialWorldState candidate;
        candidate.version =
            root.value("version", 1);

        if (root.contains("player_start") &&
            root["player_start"].is_object())
        {
            candidate.playerStart =
                readPlayerStart(root["player_start"]);
        }

        if (root.contains("system_states") &&
            root["system_states"].is_array())
        {
            for (const auto& systemJson : root["system_states"])
            {
                if (!systemJson.is_object())
                    continue;

                auto systemState = readSystemState(systemJson);
                if (systemState.systemId >= 0)
                    candidate.systemStates.push_back(std::move(systemState));
            }
        }

        if (root.contains("orbital_hubs") && root["orbital_hubs"].is_array())
        {
            for (const auto& hubJson : root["orbital_hubs"])
            {
                if (!hubJson.is_object())
                    continue;

                auto hub =
                    readOrbitalHub(hubJson);

                if (!hub.id.empty())
                    candidate.orbitalHubs.push_back(std::move(hub));
            }
        }

        std::string validationError;
        if (!validateInitialWorldState(candidate, validationError))
        {
            std::cerr
                << "[InitialWorldState] invalid " << path
                << ": " << validationError << "\n";
            return false;
        }

        out = std::move(candidate);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[InitialWorldState] failed to read "
            << path
            << ": "
            << e.what()
            << "\n";

        return false;
    }
}

inline bool loadInitialWorldStateWithFallbacks(
    InitialWorldState& out
)
{
    if (loadInitialWorldState(
            "assets/data/initial_world_state.json",
            out
        ))
    {
        return true;
    }

    if (loadInitialWorldState(
            "../assets/data/initial_world_state.json",
            out
        ))
    {
        return true;
    }

    std::cerr
        << "[InitialWorldState] cannot find initial_world_state.json\n";

    return false;
}

} // namespace game::world_state