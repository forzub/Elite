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
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
