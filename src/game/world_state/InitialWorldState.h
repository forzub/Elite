#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace game::world_state
{

struct InitialWorldStateMotion
{
    std::string type = "fixed";
    // "fixed"  — использовать orbital_period_seconds из JSON.
    // "kepler" — вычислить период по массе родительского тела и высоте орбиты.
    std::string orbitalPeriodMode = "fixed";

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
    double hull = 1.0;
    double power = 1.0;
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

struct InitialWorldState
{
    int version = 1;
    double epochUniverseTimeSeconds = 0.0;

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
        
    out.orbitalPeriodMode =
        j.value("orbital_period_mode", "fixed");

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

        out.hull =
            state.value("hull", 1.0);

        out.power =
            state.value("power", 1.0);
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

        out.version =
            root.value("version", 1);

        out.epochUniverseTimeSeconds =
            root.value("epoch_universe_time_seconds", 0.0);

        if (root.contains("orbital_hubs") && root["orbital_hubs"].is_array())
        {
            for (const auto& hubJson : root["orbital_hubs"])
            {
                if (!hubJson.is_object())
                    continue;

                auto hub =
                    readOrbitalHub(hubJson);

                if (!hub.id.empty())
                    out.orbitalHubs.push_back(std::move(hub));
            }
        }

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