#include "GalaxyDatabase.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

#include <iostream>

using json = nlohmann::json;

static json loadJsonFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
        throw std::runtime_error("Cannot open file: " + path);

    json j;
    f >> j;
    return j;
}



void GalaxyDatabase::loadFromDirectory(const std::string& path)
{
    loadActors(path + "/actors.json");
    loadSubActors(path + "/subactors.json");
    loadSystems(path + "/systems.json");
    loadNodes(path + "/nodes.json");
    loadRoutes(path + "/routes.json");

    validate();
}




void GalaxyDatabase::loadActors(const std::string& file)
{
    auto j = loadJsonFile(file);

    for (const auto& a : j["actors"])
    {
        Actor actor;
        actor.id          = a["id"];
        actor.name        = a["name"];
        actor.kind        = static_cast<ActorKind>(a["kind"]);
        actor.tier        = static_cast<ActorTier>(a["tier"]);
        actor.culture     = a.value("culture", "");
        actor.description = a.value("description", "");
        actor.subActors.clear();

        if (m_actors.find(actor.id) != m_actors.end())
            throw std::runtime_error("Duplicate ActorId: " + std::to_string(actor.id));

        m_actors.emplace(actor.id, std::move(actor));
    }
}



void GalaxyDatabase::loadSubActors(const std::string& file)
{
    auto j = loadJsonFile(file);

    for (const auto& s : j["subactors"])
    {
        SubActor sub;
        sub.id          = s["id"];
        sub.parent      = s["parent"];
        sub.name        = s["name"];
        sub.description = s.value("description", "");

        if (m_actors.find(sub.parent) == m_actors.end())
            throw std::runtime_error("SubActor references unknown ActorId");

        if (m_subActors.find(sub.id) != m_subActors.end())
            throw std::runtime_error("Duplicate SubActorId: " + std::to_string(sub.id));

        m_subActors.emplace(sub.id, sub);
        m_actors[sub.parent].subActors.push_back(sub.id);
    }
}


void GalaxyDatabase::loadSystems(const std::string& file)
{
    auto j = loadJsonFile(file);

    for (const auto& s : j["systems"])
    {
        StarSystem sys;
        sys.id   = s["id"];
        sys.name = s["name"];
        sys.position = {
            s["position"][0],
            s["position"][1],
            s["position"][2]
        };
        // sys.distance = s["distance"];
        sys.kind     = static_cast<StarSystemKind>(s["kind"]);
        sys.dominantActor = s.value("dominantActor", InvalidActor);
        sys.nodes.clear();

        if (sys.dominantActor != InvalidActor &&
            m_actors.find(sys.dominantActor) == m_actors.end())
        {
            throw std::runtime_error("System references unknown ActorId");
        }

        if (m_systems.find(sys.id) != m_systems.end())
            throw std::runtime_error("Duplicate StarSystemId: " + std::to_string(sys.id));

        m_systems.emplace(sys.id, std::move(sys));
    }
}




void GalaxyDatabase::loadNodes(const std::string& file)
{
    auto j = loadJsonFile(file);

    for (const auto& n : j["nodes"])
    {
        Node node;
        node.systemId = n.value("systemId", InvalidStarSystem);

        node.population = n.value("population", -1LL);
        node.securityLevel = n.value("securityLevel", -1.0f);
        node.techLevel     = n.value("techLevel", -1.0f);
        node.economicValue = n.value("economicValue", -1.0f);
        node.id        = n["id"];
        node.name      = n["name"];
        node.type      = static_cast<NodeType>(n["type"]);
        node.role      = static_cast<NodeRole>(n["role"]);
        node.owner     = n["owner"];
        node.controller= n.value("controller", InvalidSubActor);

        node.roleText     = n.value("roleText", "");
        node.locationText = n.value("locationText", "");
        node.description  = n.value("description", "");

        node.tags.clear();
        if (n.contains("tags"))
        {
            for (const auto& t : n["tags"])
                node.tags.push_back(t);
        }


        if (m_nodes.find(node.id) != m_nodes.end())
            throw std::runtime_error("Duplicate NodeId: " + std::to_string(node.id));

        if (node.systemId != InvalidStarSystem &&
            m_systems.find(node.systemId) == m_systems.end())
        {
            throw std::runtime_error("Node references unknown StarSystem");
        }

        if (m_actors.find(node.owner) == m_actors.end())
            throw std::runtime_error("Node references unknown Actor");

        if (node.controller != InvalidSubActor &&
            m_subActors.find(node.controller) == m_subActors.end())
        {
            throw std::runtime_error("Node references unknown SubActor");
        }    

        m_nodes.emplace(node.id, node);

        if (node.systemId != InvalidStarSystem)
        {
            m_systems[node.systemId].nodes.push_back(node.id);
        }
    }
}





void GalaxyDatabase::loadRoutes(const std::string& file)
{
    auto j = loadJsonFile(file);

    for (const auto& r : j["routes"])
    {
        StarRoute route;
        route.id         = r["id"];
        route.a          = r["a"];
        route.b          = r["b"];
        route.distance   = r["distance"];
        route.restricted = r.value("restricted", false);
        route.dangerLevel= r.value("dangerLevel", 0.0f);

        if (m_systems.find(route.a) == m_systems.end())
        {
            std::cerr << "Missing StarSystem A: " << route.a << "\n";
        }
        if (m_systems.find(route.b) == m_systems.end())
        {
            std::cerr << "Missing StarSystem B: " << route.b << "\n";
        }



        if (m_routes.find(route.id) != m_routes.end())
            throw std::runtime_error("Duplicate RouteId");

        if (m_systems.find(route.a) == m_systems.end() ||
            m_systems.find(route.b) == m_systems.end())
        {
            throw std::runtime_error("Route references unknown StarSystem");
        }

        m_routes.emplace(route.id, route);
    }
}


// void GalaxyDatabase::loadGenerationShips(const std::string& file)
// {
//     auto j = loadJsonFile(file);

//     for (const auto& g : j["generationShips"])
//     {
//         GenerationShip ship;

//         ship.id            = g["id"];
//         ship.name          = g["name"];
//         ship.className     = g["className"];
//         ship.lengthMeters  = g["lengthMeters"];
//         ship.massTons      = g.value("massTons", -1.0f);

//         ship.launchYear    = g["launchYear"];

//         if (g.contains("arrivalYear") && !g["arrivalYear"].is_null())
//             ship.arrivalYear = g["arrivalYear"];
//         else
//             ship.arrivalYear.reset();

//         ship.originActor = g.value("originActor", InvalidActor);
//         ship.originNode  = g.value("originNode", InvalidNode);

//         ship.plannedRoute.clear();
//         if (g.contains("plannedRoute"))
//         {
//             for (auto sid : g["plannedRoute"])
//                 ship.plannedRoute.push_back(sid);
//         }

//         if (g.contains("targetSystem") && !g["targetSystem"].is_null())
//             ship.targetSystem = g["targetSystem"];
//         else
//             ship.targetSystem.reset();

//         ship.targetDescription = g.value("targetDescription", "");

//         ship.initialPopulation = g.value("initialPopulation", 0);

//         ship.status3024 =
//             static_cast<GenerationShipStatus>(g.value("status3024", 0));

//         ship.programDescription = g.value("programDescription", "");
//         ship.fateDescription    = g.value("fateDescription", "");

//         if (g.contains("currentSystem") && !g["currentSystem"].is_null())
//             ship.currentSystem = g["currentSystem"];
//         else
//             ship.currentSystem.reset();

//         if (g.contains("currentRoute") && !g["currentRoute"].is_null())
//             ship.currentRoute = g["currentRoute"];
//         else
//             ship.currentRoute.reset();

//         ship.routeProgress = g.value("routeProgress", -1.0f);

//         ship.emitsSignal   = g.value("emitsSignal", false);
//         ship.questRelevant = g.value("questRelevant", false);

//         ship.tags.clear();
//         if (g.contains("tags"))
//         {
//             for (const auto& t : g["tags"])
//                 ship.tags.push_back(t);
//         }

//         if (m_generationShips.contains(ship.id))
//             throw std::runtime_error("Duplicate GenerationShipId");

//         // --- базовая валидация ---
//         if (ship.lengthMeters <= 0.0f)
//             throw std::runtime_error("GenerationShip with invalid size");

//         if (ship.originActor != InvalidActor &&
//             !m_actors.contains(ship.originActor))
//             throw std::runtime_error("GenerationShip references unknown Actor");

//         m_generationShips.emplace(ship.id, std::move(ship));
//     }
// }





void GalaxyDatabase::validate() const
{
    // 1. Каждая система должна ссылаться только на существующие узлы
    for (const auto& [sysId, sys] : m_systems)
    {
        for (NodeId nid : sys.nodes)
        {
            if (m_nodes.find(nid) == m_nodes.end())
            {
                throw std::runtime_error(
                    "System " + std::to_string(sysId) +
                    " contains invalid NodeId " + std::to_string(nid)
                );
            }
        }
    }

    // 2. Узлы должны ссылаться на существующие системы (если systemId задан)
    for (const auto& [nodeId, node] : m_nodes)
    {
        if (node.systemId != InvalidStarSystem &&
            m_systems.find(node.systemId) == m_systems.end())
        {
            throw std::runtime_error(
                "Node " + std::to_string(nodeId) +
                " references invalid systemId " +
                std::to_string(node.systemId)
            );
        }
    }

    // 3. Сабакторы должны принадлежать существующим акторам
    for (const auto& [subId, sub] : m_subActors)
    {
        if (m_actors.find(sub.parent) == m_actors.end())
        {
            throw std::runtime_error(
                "SubActor " + std::to_string(subId) +
                " references invalid parent ActorId " +
                std::to_string(sub.parent)
            );
        }
    }
}

