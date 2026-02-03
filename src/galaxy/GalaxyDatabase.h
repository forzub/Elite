#pragma once
#include "src/galaxy/Actors.h"
#include "src/galaxy/SubActor.h"
#include "src/galaxy/Node.h"
#include "src/galaxy/StarSystem.h"
#include "src/galaxy/StarRoute.h"

#include <unordered_map>
#include <string>

class GalaxyDatabase
{
public:
    void loadFromDirectory(const std::string& path);

    void validate() const;

    const Actor*      getActor(ActorId id) const;
    const SubActor*   getSubActor(SubActorId id) const;
    const Node*       getNode(NodeId id) const;
    const StarSystem* getSystem(StarSystemId id) const;
    const StarRoute*  getRoute(RouteId id) const;

    // диагностические геттеры (без доступа к контейнерам)
    size_t actorCount()  const noexcept { return m_actors.size(); }
    size_t systemCount() const noexcept { return m_systems.size(); }
    size_t nodeCount()   const noexcept { return m_nodes.size(); }
    size_t routeCount()  const noexcept { return m_routes.size(); }

private:
    void loadActors(const std::string& file);
    void loadSubActors(const std::string& file);
    void loadSystems(const std::string& file);
    void loadNodes(const std::string& file);
    void loadRoutes(const std::string& file);
    // void loadGenerationShips(const std::string& file);

private:
    std::unordered_map<ActorId, Actor>           m_actors;
    std::unordered_map<SubActorId, SubActor>     m_subActors;
    std::unordered_map<NodeId, Node>             m_nodes;
    std::unordered_map<StarSystemId, StarSystem> m_systems;
    std::unordered_map<RouteId, StarRoute>       m_routes;
    // std::unordered_map<GenerationShipId, GenerationShip> m_generationShips;

    
};
