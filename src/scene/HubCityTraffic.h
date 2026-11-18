#pragma once

// The traffic of a planar hub city (author 2026-11-18: «надо сделать трафик - сгенерировать каких-то типа машин,
// грузовичков, пассажирских поездов»): a presentation only - no server state. Every vehicle is a pure function of the
// universe time and the hub plan (assets/data/navigation/hub_plans/<hub>.json), so every client sees the same traffic:
//  - cars and trucks on the road bridges of every street, over the plane and (upside down) under it, two lanes each
//    way (right-hand traffic); trucks only beside the wheeled zones (stores, industry, port...), on the outer lane;
//    a vehicle leaving the street at its far crossing comes back in at the near one (the streams look continuous);
//  - a 3-car monorail train on every monorail beam, shuttling between the street's ends with a dwell at each.
// Models: assets/compiled/hub_modules/hubt_* (tools/models/units/hub_traffic.py), +Z forward, +Y up.
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{
class HubPlan;
}

namespace scene
{
enum class HubTrafficModel : int
{
    CarA = 0,
    CarB,
    CarC,
    Truck,
    Train,
    Count
};

// the package folder of a traffic model ("hubt_car_a" ...)
const char* hubTrafficModelId(HubTrafficModel model);

struct HubTrafficInstance
{
    HubTrafficModel model = HubTrafficModel::CarA;
    glm::mat4 local {1.0f};     // the vehicle in the city frame (the plan's metres)
    float distance = 0.0f;      // to the camera
};

// the vehicles of the city at timeSeconds within radius of the camera (city frame) -> out (cleared first)
void hubCityTraffic(const game::navigation::HubPlan& plan, double timeSeconds, const glm::vec3& cameraLocal, float radius,
                    std::vector<HubTrafficInstance>& out);

// The city's AIR traffic (author 2026-11-18: «воздушный трафик из фронтиров и др. их там не обязательно делать слишком
// много, но они должны быть» + «разные корабли в рамках политического блока к которому принадлежит хаб»): the ships
// of the hub plan's air fleet (its bloc's hulls) flying the green lanes and the blue dock approaches of the hub's lane
// network (assets/data/navigation/hub_lanes), one to three per lane, at 70 % of the lane's speed; a ship appears at
// the lane's open-sky portal and leaves at its end. Each ship is a fleet index (the plan's air_fleet), nose -Z.
struct HubAirTrafficInstance
{
    int fleetIndex = 0;
    glm::mat4 local {1.0f};
    float distance = 0.0f;
};
}

namespace game::navigation::traffic
{
class HubLaneNetwork;
}

namespace scene
{
void hubCityAirTraffic(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork& lanes,
                       double timeSeconds, const glm::vec3& cameraLocal, float radius, std::vector<HubAirTrafficInstance>& out);
} // namespace scene
