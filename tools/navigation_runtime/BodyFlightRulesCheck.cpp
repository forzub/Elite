// Offline check of the shared body flight rules and the route codec (architecture audit 2026-11-06):
// the dominant body (Laplace sphere, catalog parent), the J floor, the clearance, the airless rule,
// the route encode / decode round trip and the J charge estimate. Exit 1 on any failure.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "src/game/navigation/route/RoutePlanner.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/navigation/rules/JDrive.h"
#include "src/game/navigation/rules/SensorSpeedRules.h"

namespace
{
int failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok)
        ++failures;
}

struct Body
{
    std::string id, parent;
    double mu;
    glm::dvec3 c;
};
} // namespace

int main()
{
    namespace r = game::navigation::rules;
    namespace jd = game::navigation::jdrive;
    const double au = 1.495978707e11;
    // Sun, Earth at 1 AU, the Moon 384 400 km from the Earth
    const std::vector<Body> bodies {
        {"system_0.Sol", "", 1.32712440018e20, glm::dvec3(0.0)},
        {"system_0.Sol.Земля", "system_0.Sol", 3.986004418e14, glm::dvec3(au, 0.0, 0.0)},
        {"system_0.Sol.Земля.Луна", "system_0.Sol.Земля", 4.9048695e12, glm::dvec3(au + 3.844e8, 0.0, 0.0)},
    };
    const auto pick = [&](const glm::dvec3& p, bool* root = nullptr) {
        const std::size_t k = r::dominantBody(bodies, p, [](const Body& b) -> const std::string& { return b.id; },
                                              [](const Body& b) { return b.parent; }, [](const Body& b) { return b.mu; },
                                              [](const Body& b) { return b.c; }, root);
        return k < bodies.size() ? bodies[k].id : std::string("none");
    };
    // the same with no catalog parents (the server's GravityBody): the id prefix gives the parent
    const auto pickPrefix = [&](const glm::dvec3& p) {
        const std::size_t k = r::dominantBody(bodies, p, [](const Body& b) -> const std::string& { return b.id; },
                                              [](const Body&) { return std::string(); }, [](const Body& b) { return b.mu; },
                                              [](const Body& b) { return b.c; });
        return bodies[k].id;
    };
    const glm::dvec3 leo(au + 6.8e6, 0.0, 0.0), lunar(au + 3.844e8 + 1.84e6, 0.0, 0.0), l1(au + 3.0e8, 0.0, 0.0),
        deep(au * 1.5, 0.0, 0.0);
    bool root = false;
    check(pick(leo) == "system_0.Sol.Земля", "LEO -> Earth");
    check(pick(lunar) == "system_0.Sol.Земля.Луна", "low lunar orbit -> Moon");
    check(pick(l1) == "system_0.Sol.Земля", "300 000 km from the Earth (84 000 km from the Moon, outside its 66 000 km) -> Earth");
    check(pick(deep, &root) == "system_0.Sol" && root, "1.5 AU -> the star (root)");
    check(pickPrefix(lunar) == pick(lunar) && pickPrefix(leo) == pick(leo), "catalog parent == id-prefix parent");

    // J floor, clearance, airless
    check(std::abs(r::floorAltitudeM(1737.4e3, 1737.4e3) - 86870.0) < 1.0, "Moon J floor 86.87 km");
    check(std::abs(r::floorAltitudeM(6371e3, 6371e3 + 100e3) - 318550.0) < 1.0, "Earth J floor 318.6 km (atmosphere + 50 km / 5 % R)");
    check(r::clearanceM(696340e3, 696340e3, true) == 20.0 * 696340e3, "the star: 20 radii");
    check(std::abs(r::clearanceM(6371e3, 6471e3, false) - (6371e3 + 0.05 * 6371e3 + 10e3)) < 1.0,
          "Earth: max(R + 150 km, atmosphere + 50 km, R + J floor (5 % R) + 10 km)");
    check(r::clearanceM(1737e3, 1737e3, false) == 1737e3 + 150e3, "Moon: R + 150 km (above its 87 km J floor)");
    check(r::isAirless(1737.4e3, 1737.4e3) && !r::isAirless(6371e3, 6471e3), "airless: the Moon yes, the Earth no");

    // route codec round trip (a settlement pad spec carries ':')
    namespace route = game::navigation::route;
    route::PlannedRoute planned;
    route::RoutePoint a;
    a.kind = route::RoutePoint::Kind::Hub;
    a.hubId = "earth_orbital_hub";
    a.stop = true;
    a.action = "refuel+repair";
    route::RoutePoint b;
    b.kind = route::RoutePoint::Kind::Land;
    b.bodyId = "system_0.Sol.Земля.Луна";
    b.landingSpec = "site:site.000.sol.shackleton";
    b.action = "pause";
    route::RoutePoint c;
    c.kind = route::RoutePoint::Kind::Orbit;
    c.bodyId = "system_0.Sol.Земля";
    c.orbitAltitudeM = 500000.0;
    c.stop = true;
    route::RoutePoint d;
    d.kind = route::RoutePoint::Kind::Ship;
    d.shipEntityId = 42;
    planned.legs = {a, b, c, d};
    const auto back = route::decodeRoute(route::encodeRoute(planned));
    check(back.size() == 4, "codec: 4 legs back");
    if (back.size() == 4)
    {
        check(back[0].kind == route::RoutePoint::Kind::Hub && back[0].hubId == a.hubId && back[0].action == a.action, "codec: hub + actions");
        check(back[1].kind == route::RoutePoint::Kind::Land && back[1].landingSpec == b.landingSpec && back[1].action == "pause",
              "codec: landing pad spec with ':'");
        check(back[2].kind == route::RoutePoint::Kind::Orbit && std::abs(back[2].orbitAltitudeM - 500000.0) < 1e-6, "codec: orbit altitude (m)");
        check(back[3].kind == route::RoutePoint::Kind::Ship && back[3].shipEntityId == 42, "codec: rendezvous ship id");
    }

    // charge: grows with the distance, the coast at the cap is free, a lighter core load is cheaper
    const double c1 = route::estimateLegChargeT(1.0e9, 260.0, 500.0);
    const double c2 = route::estimateLegChargeT(1.0e10, 260.0, 500.0);
    const double c3 = route::estimateLegChargeT(1.0e13, 260.0, 500.0);
    const double c4 = route::estimateLegChargeT(1.0e14, 260.0, 500.0);
    check(c1 > 0.0 && c2 > c1, "charge grows with distance");
    check(std::abs(c3 - c4) < 1e-9, "coast at 0.9 c costs nothing");
    check(jd::jLegRatio(150.0, 500.0) < jd::jLegRatio(480.0, 500.0), "a heavier load of the core costs more");

    // sensor-limited speed (docs/SENSOR_LIMITED_SPEED.md): the doc's table and the charted rule
    {
        namespace sn = game::navigation::rules::sensor;
        const double v1 = sn::sensorSpeedLimitMps(2.0e6, 1.5, 1.0e6);
        const double v2 = sn::sensorSpeedLimitMps(2.0e6, 1.5, 1.0e4);
        const double v3 = sn::sensorSpeedLimitMps(1.5e5, 2.0, 1.0e4);
        std::printf("      sensor v_max: 2000 km 1.5 s 1e6 -> %.1f km/s; 1e4 -> %.1f km/s; 150 km 2 s 1e4 -> %.1f km/s; full J %.1f km/s\n",
                    v1 / 1000.0, v2 / 1000.0, v3 / 1000.0, sn::sensorSpeedLimitMps(2.0e6, 1.5, jd::MaxAccelMps2) / 1000.0);
        check(std::abs(v1 - 834.5e3) < 1.0e3, "sensor: 2000 km, 1.5 s, 1e6 m/s2 -> ~834 km/s");
        check(std::abs(v2 - 164.5e3) < 1.0e3, "sensor: 2000 km, 1.5 s, 1e4 m/s2 -> ~164 km/s");
        check(std::abs(v3 - 32.9e3) < 0.5e3, "sensor: 150 km optics, 2 s, 1e4 m/s2 -> ~33 km/s");
        // the bound is the stopping law itself: reaction run + braking run = k R
        check(std::abs(v1 * 1.5 + v1 * v1 / 2.0e6 - 0.8 * 2.0e6) < 1.0, "sensor: v t + v^2/2a = k R");
        check(sn::sensorSpeedLimitMps(2.0e6, 3.0, 1.0e6) < v1, "sensor: a failed nav computer (longer reaction) is slower");
        check(sn::sensorSpeedLimitMps(5.0e7, 1.5, 1.0e6) > v1, "sensor: a longer radar range is faster");
        check(sn::sensorSpeedLimitMps(2.0e6, 1.5, 0.0) == 0.0 && sn::sensorSpeedLimitMps(0.0, 1.5, 1.0e6) == 0.0,
              "sensor: no braking / no range -> 0");
        check(std::abs(sn::sensorSpeedLimitMps(2.0e6, 1.5, 1.0e30) - 0.8 * 2.0e6 / 1.5) < 1.0, "sensor: unlimited braking -> k R / t_r");
        check(sn::sensorRangeM(0.0) == sn::OpticsRangeM && sn::sensorRangeM(1000.0) == sn::OpticsRangeM && sn::sensorRangeM(2.0e6) == 2.0e6,
              "sensor: no radar / a 1 km docking radar -> optics range");
        sn::ChartedQuery q;
        q.catalogueSystem = true;
        q.farthestBodyM = 30.0 * au;
        q.hubZoneM = 3.0e6;
        q.distanceFromStarM = 40.0 * au;
        check(sn::chartedBy(q) == sn::ChartedBy::None, "uncharted: an EMPTY catalogue system (no places / hubs) is known only in fragments");
        q.inhabited = true;
        check(sn::chartedBy(q) == sn::ChartedBy::SurveyShell, "charted: 40 AU in an inhabited 30 AU system (1.5 x shell)");
        q.distanceFromStarM = 60.0 * au;
        check(sn::chartedBy(q) == sn::ChartedBy::None, "uncharted: 60 AU, beyond the shell");
        q.nearestHubM = 2.0e6;
        check(sn::chartedBy(q) == sn::ChartedBy::HubZone, "charted: a hub's radar zone beyond the shell");
        q.nearestHubM = 1.0e9;
        q.inGreenCorridor = true;
        check(sn::chartedBy(q) == sn::ChartedBy::Corridor, "charted: a green corridor");
        sn::ChartedQuery deep;   // a deep-space place / approaches: no shell
        deep.distanceFromStarM = 1.0;
        check(sn::chartedBy(deep) == sn::ChartedBy::None, "uncharted: deep space / approaches without records");
        deep.approachLaneRecord = true;
        check(sn::chartedBy(deep) == sn::ChartedBy::LaneRecord, "charted: approaches with a usable lane record");

        // variant 1 (author 2026-11-20): uncharted space is bound only in / near a hazard
        sn::HazardZone belt;   // Sol's main belt: 2.06 - 3.27 AU, half-height 2 x 0.456 AU
        belt.kind = sn::HazardKind::Belt;
        belt.normal = glm::dvec3(0.0, 1.0, 0.0);
        belt.innerM = 2.06 * au;
        belt.outerM = 3.27 * au;
        belt.halfThicknessM = 2.0 * 0.456 * au;
        check(sn::hazardDistanceM(belt, glm::dvec3(2.5 * au, 0.3 * au, 0.0)) == 0.0, "hazard: inside the belt");
        check(std::abs(sn::hazardDistanceM(belt, glm::dvec3(0.0, 0.0, 1.96 * au)) - 0.1 * au) < 1.0e3, "hazard: 0.1 AU inside the belt's inner edge");
        check(std::abs(sn::hazardDistanceM(belt, glm::dvec3(2.5 * au, 1.912 * au, 0.0)) - 1.0 * au) < 1.0e3, "hazard: 1 AU above the belt");
        sn::HazardZone trojans = belt;   // an arc +-34 deg around +x
        trojans.arcCentreDir = glm::dvec3(1.0, 0.0, 0.0);
        trojans.arcHalfRad = sn::TrojanHalfArcRad;
        check(sn::hazardDistanceM(trojans, glm::dvec3(2.5 * au, 0.0, 0.5 * au)) == 0.0, "hazard: inside a trojan arc");
        check(sn::hazardDistanceM(trojans, glm::dvec3(-2.5 * au, 0.0, 0.0)) > 5.0 * au, "hazard: the far side of the orbit is clear of the trojans");
        sn::HazardZone body;   // the Earth: a sphere of 10 R
        body.kind = sn::HazardKind::Body;
        body.outerM = sn::NearBodyRadii * 6.371e6;
        check(sn::hazardDistanceM(body, glm::dvec3(4.2164e7, 0.0, 0.0)) == 0.0, "hazard: geostationary orbit is near the Earth (10 R)");
        check(std::abs(sn::hazardDistanceM(body, glm::dvec3(3.844e8, 0.0, 0.0)) - (3.844e8 - 6.371e7)) < 1.0, "hazard: the Moon's distance is 320 000 km clear");
        double d = 0.0;
        check(sn::nearestHazard({belt, body}, glm::dvec3(1.0e8, 0.0, 0.0), d) == 1 && std::abs(d - (1.0e8 - 6.371e7)) < 1.0, "hazard: the nearest zone");
        check(sn::nearestHazard({}, glm::dvec3(0.0), d) == -1 && std::isinf(d), "hazard: no data -> none");
        // the margin: the stopping run at the speed now (at rest only inside counts)
        const double brake = sn::HazardBrakeFraction * jd::MaxAccelMps2;
        const double run = sn::stoppingDistanceM(1.0e8, 1.5, brake);
        std::printf("      hazard margin: 100 000 km/s -> %.3f AU; 0.9 c -> %.3f AU; v_max 77.9 km/s -> %.0f km\n", run / au,
                    sn::stoppingDistanceM(jd::SpeedCapMps, 1.5, brake) / au, sn::stoppingDistanceM(77.9e3, 1.5, brake) / 1000.0);
        check(std::abs(run - (1.5e8 + 1.0e16 / (2.0 * brake))) < 1.0, "hazard: stopping run v t + v^2/2a");
        check(!sn::inHazard(0.1 * au, 0.0, 1.5, brake) && sn::inHazard(0.0, 0.0, 1.5, brake), "hazard: at rest 0.1 AU off - free; inside - bound");
        check(!sn::inHazard(0.1 * au, 3.0e7, 1.5, brake), "hazard: 30 000 km/s, 0.1 AU off - free (stops in 0.008 AU)");
        check(sn::inHazard(0.1 * au, 1.5e8, 1.5, brake), "hazard: 150 000 km/s, 0.1 AU off - bound (stops in 0.19 AU)");
        check(!sn::inHazard(1.0 * au, jd::SpeedCapMps, 1.5, brake), "hazard: 0.9 c, 1 AU off - free (stops in 0.62 AU)");
        check(std::isinf(sn::stoppingDistanceM(10.0, 1.5, 0.0)) && sn::stoppingDistanceM(0.0, 1.5, 0.0) == 0.0, "hazard: no braking -> never stops");
        // the envelope: v_max at the edge (and inside), faster further off; the speed it allows keeps the ship "near"
        const double vmax = sn::sensorSpeedLimitMps(sn::OpticsRangeM, 1.5, jd::MaxAccelMps2);
        check(std::abs(sn::hazardApproachSpeedMps(vmax, 0.0, brake) - vmax) < 1e-9 && std::abs(sn::hazardApproachSpeedMps(vmax, -5.0, brake) - vmax) < 1e-9,
              "hazard envelope: v_max at the edge / inside");
        bool latched = true;
        for (double dd : {1.0e3, 1.0e6, 1.0e9, 0.1 * au, 1.0 * au, 30.0 * au})
            latched = latched && sn::inHazard(dd, sn::hazardApproachSpeedMps(vmax, dd, brake), 1.5, brake);
        check(latched, "hazard envelope: flying the envelope stays within the margin (no on / off chatter)");
        check(std::abs(sn::hazardApproachSpeedMps(vmax, 0.1 * au, brake) - std::sqrt(vmax * vmax + 2.0 * brake * 0.1 * au)) < 1e-6,
              "hazard envelope: sqrt(v_max^2 + 2 a d)");
    }
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
