#pragma once

// The bustle around the construction sites of a hub city (author 2026-11-19: «обеспечить визуальную возню вокруг этих
// модулей из транспортов и челноков, создать временные платформы для загрузки / разгрузки» + the correction «краны -
// только на планетах и там где есть гравитация. на орбитальном и пространственном хабе ... компактные тягачи,
// возможно - ассистед корабль с парой больших клешней впереди, для удержания и контроля подачи конструкции»).
// A presentation only, like HubCityTraffic.h: every craft is a pure function of the universe time and the hub's
// construction plan (src/model_asset/HubExpansion.h), so every client sees the same:
//  - a cargo hauler per temporary deck: arrives, sits on the deck while it is unloaded, leaves;
//  - claw ships: take an element (a beam / a panel) at a deck, carry it on an arc to a build-front point, approach
//    slowly, hold it while it is fixed, release, back off and return empty; two tugs escort each;
//  - tethered worker pods drifting at the build front;
//  - in gravity (a surface site) the jibs of the tower cranes slewing over their masts.
// A halted / abandoned site has no traffic: a tug parked on each deck, the crane jibs left weathervaning.
// Models: assets/compiled/hub_modules/hubc_* (tools/models/units/hub_construction.py), +Z forward, +Y up.

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace scene
{
enum class ConstructionCraft : int
{
    Tug = 0,
    ClawShip,
    Beam,
    Panel,
    Pod,
    Hauler,
    CraneJib,
    Count
};

const char* constructionCraftModelId(ConstructionCraft model);

struct ConstructionTrafficInstance
{
    ConstructionCraft model = ConstructionCraft::Tug;
    glm::mat4 local {1.0f};     // in the city frame (the layout's metres)
    float distance = 0.0f;      // to the camera
};

// the construction craft of hub `hubId` at timeSeconds (universe seconds) within radius of the camera (city frame)
// -> appended to out; nothing when the hub has no plan or no site under way
void hubConstructionTraffic(const std::string& hubId, double timeSeconds, const glm::vec3& cameraLocal, float radius,
                            std::vector<ConstructionTrafficInstance>& out);
} // namespace scene
