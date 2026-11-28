// Core stabilization D-5: the client's control logic reads the simulation state, never the presentation
// (PROJECT_STATE.md E-10). ClientControlState.h combines the own ship (predicted in its frame) with other bodies (at
// the snapshot) at the own epoch.
//
// Checked:
// - the presentation fields (renderTransform, renderWorldPosition, renderOrientation) do not change any result;
// - a body at rest in the hub frame stays put when advanced (no drift with the frame's orbit, ~7.7 km/s);
// - a ring city turns by its spin relative to the frame;
// - a ship moving in the frame advances by its frame-relative velocity;
// - with no prediction (ahead 0) nothing moves.

#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/client/ClientControlState.h"

namespace cs = game::client::control;

namespace
{
int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}
double dist(const glm::dvec3& a, const glm::dvec3& b) { return glm::length(a - b); }

game::navigation::KinematicFrame hubFrame()
{
    game::navigation::KinematicFrame f;
    f.systemId = 3;
    f.frameId = "hub";
    f.originMeters = glm::dvec3(6.9e6, 1.0e5, 0.0);
    f.linearVelocityMps = glm::dvec3(0.0, 7660.0, 0.0);
    f.localToWorldBasis = glm::dmat3(glm::rotate(glm::dmat4(1.0), 0.4, glm::dvec3(0.0, 0.0, 1.0)));
    f.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, 1.13e-3);
    f.valid = true;
    return f;
}

void scramblePresentation(ClientObjectState& o)
{
    o.renderWorldPosition = world::coordinates::makeWorldPositionFromMeters(glm::dvec3(-1.0e9, 3.0e8, 7.0e7));
    o.renderOrientation = glm::rotate(glm::mat4(1.0f), 2.1f, glm::vec3(1.0f, 0.0f, 0.0f));
    o.renderLinearVelocityMps = glm::dvec3(1.0e5);
}

void scramblePresentation(ClientShipState& s)
{
    s.renderTransform.setWorldPositionMeters(glm::dvec3(5.0e9, -2.0e9, 1.0));
    s.renderTransform.orientation = glm::rotate(glm::mat4(1.0f), -1.3f, glm::vec3(0.0f, 1.0f, 0.0f));
    s.renderTransform.motion.worldVelocityMps = glm::dvec3(-9.9e4);
}
} // namespace

int main()
{
    const auto K = hubFrame();
    const double ahead = 0.12;   // ~6 fixed steps of prediction
    cs::View view;
    view.frame = K;
    view.aheadSeconds = ahead;

    // a city at rest in the hub frame (it rides the hub's orbit) - its world velocity is the frame's at its point
    ClientObjectState city;
    const glm::dvec3 cityLocal(1500.0, -300.0, 220.0);
    city.worldPosition = world::coordinates::makeWorldPositionFromMeters(K.localToWorldPosition(cityLocal));
    city.linearVelocityMps = K.localToWorldVelocity(cityLocal, glm::dvec3(0.0));
    city.orientation = glm::mat4(glm::mat3(K.localToWorldBasis));
    city.angularVelocityWorldRadPerSecond = K.angularVelocityWorldRadPerSecond;   // turns with the frame only

    const cs::BodyState c = cs::object(view, city);
    check(dist(c.positionMeters, K.localToWorldPosition(cityLocal)) < 1.0e-6,
          "a city at rest in the frame stays at its frame point (no drift with the orbit)");
    check(std::abs(glm::determinant(c.orientation - glm::dmat3(glm::mat3(city.orientation)))) < 1.0e-12 &&
              dist(c.orientation[0], glm::dvec3(glm::mat3(city.orientation)[0])) < 1.0e-6,
          "a city turning only with the frame keeps its axes relative to the frame");

    // the same city scrambled in presentation: identical result
    ClientObjectState drawn = city;
    scramblePresentation(drawn);
    const cs::BodyState c2 = cs::object(view, drawn);
    check(c2.positionMeters == c.positionMeters && c2.orientation == c.orientation,
          "the drawn city's pose does not change the control state");

    // a ring city: spins 0.09 rad/s about its local z relative to the frame
    ClientObjectState ring = city;
    const glm::dvec3 spinAxisWorld = K.localToWorldVector(glm::dvec3(0.0, 0.0, 1.0));
    ring.angularVelocityWorldRadPerSecond = K.angularVelocityWorldRadPerSecond + spinAxisWorld * 0.09;
    const cs::BodyState r = cs::object(view, ring);
    const glm::dvec3 x0 = glm::dvec3(glm::mat3(ring.orientation)[0]);
    const double turned = std::acos(std::clamp(glm::dot(glm::normalize(r.orientation[0]), glm::normalize(x0)), -1.0, 1.0));
    char what[160];
    std::snprintf(what, sizeof what, "a ring city turns by its spin relative to the frame (%.5f vs %.5f rad)", turned,
                  0.09 * ahead);
    check(std::abs(turned - 0.09 * ahead) < 1.0e-9, what);

    // another ship moving in the frame at 40 m/s: advanced by its frame-relative velocity only
    ClientShipState other;
    const glm::dvec3 otherLocal(-800.0, 120.0, 60.0);
    const glm::dvec3 otherLocalV(40.0, 0.0, -3.0);
    other.transform.setWorldPositionMeters(K.localToWorldPosition(otherLocal));
    other.transform.motion.worldVelocityMps = K.localToWorldVelocity(otherLocal, otherLocalV);
    const cs::BodyState o = cs::otherShip(view, other);
    std::snprintf(what, sizeof what, "another ship advances by its frame-relative velocity (%.6f m off)",
                  dist(o.positionMeters, K.localToWorldPosition(otherLocal + otherLocalV * ahead)));
    check(dist(o.positionMeters, K.localToWorldPosition(otherLocal + otherLocalV * ahead)) < 1.0e-6, what);
    ClientShipState otherDrawn = other;
    scramblePresentation(otherDrawn);
    check(cs::otherShip(view, otherDrawn).positionMeters == o.positionMeters,
          "the drawn ship's pose does not change the control state");

    // the own ship: its simulation transform, whatever is drawn
    ClientShipState me;
    me.transform.setWorldPositionMeters(K.localToWorldPosition(glm::dvec3(10.0, 20.0, 30.0)));
    ClientShipState meDrawn = me;
    scramblePresentation(meDrawn);
    check(cs::ownShip(meDrawn).positionMeters == cs::ownShip(me).positionMeters,
          "the own ship's control state is its simulation transform");

    // no prediction: nothing is advanced
    const cs::View still {};
    check(cs::object(still, ring).positionMeters == glm::dvec3(world::coordinates::fullMeters(ring.worldPosition)) &&
              cs::otherShip(still, other).positionMeters == world::coordinates::fullMeters(other.transform.worldPosition),
          "ahead 0: the snapshot state as it is");

    if (g_failures == 0)
        std::printf("CLIENT CONTROL STATE: PASS\n");
    return g_failures == 0 ? 0 : 1;
}
