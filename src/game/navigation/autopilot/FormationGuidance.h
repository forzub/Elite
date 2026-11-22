#pragma once

// Joining a ship and keeping a place in its formation (author 2026-11-22: «при маршруте к кораблю ... остановка в
// безопасной для поражения зоне (приоритет заход "за спину"), приближение и постановка в строй. строй может быть
// произвольный и формация. формацию и место в ней задаёт тот корабль к которому подлетаем»).
//
// Pure guidance in one map frame (the follower's travel frame): it turns the leader's state + the formation into the
// REFERENCE the ordinary pilot flies (AutopilotControlRequest: desired velocity / course / hull pose) - the same
// controls as the pilot, no planner (a formation is station keeping, not a route).
//   Wide     - in front of the leader: first out to its side, wide (never across its bow / its guns)
//   Behind   - the safe point behind it, on the side of our slot ("за спину")
//   Close    - to the slot
//   Hold     - in the slot: the leader's velocity + a correction, the leader's attitude
#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{
enum class FormationKind : std::uint8_t
{
    Wedge = 0,     // a V behind the leader
    Echelon,       // a slanted line to the right, behind
    Column,        // one behind the other
    Diamond,       // right, left, behind (4 ships with the leader)
    Line,          // abreast
};

enum class FormationPhase : std::uint8_t
{
    Wide = 0,
    Behind,
    Close,
    Hold,
};

inline const char* formationKindName(FormationKind k)
{
    switch (k)
    {
        case FormationKind::Wedge: return "wedge";
        case FormationKind::Echelon: return "echelon";
        case FormationKind::Column: return "column";
        case FormationKind::Diamond: return "diamond";
        case FormationKind::Line: return "line";
    }
    return "?";
}

inline FormationKind formationKindFromName(const char* s)
{
    const auto is = [&](const char* n) {
        for (int i = 0;; ++i)
        {
            if (s[i] != n[i])
                return false;
            if (!s[i])
                return true;
        }
    };
    if (is("echelon")) return FormationKind::Echelon;
    if (is("column")) return FormationKind::Column;
    if (is("diamond")) return FormationKind::Diamond;
    if (is("line")) return FormationKind::Line;
    return FormationKind::Wedge;
}

// the slot offset in the leader's own axes: x = right, y = up, z = BACK (behind the leader is +z); slot 0 = the first
// follower
inline glm::dvec3 formationSlotOffset(FormationKind kind, int slot, double spacing)
{
    const int rank = slot / 2 + 1;
    const double side = (slot % 2 == 0) ? 1.0 : -1.0;
    switch (kind)
    {
        case FormationKind::Wedge:
            return glm::dvec3(side * rank * spacing * 0.7, 0.0, rank * spacing * 0.7);
        case FormationKind::Echelon:
            return glm::dvec3((slot + 1) * spacing * 0.7, 0.0, (slot + 1) * spacing * 0.7);
        case FormationKind::Column:
            return glm::dvec3(0.0, 0.0, (slot + 1) * spacing);
        case FormationKind::Diamond:
        {
            const int s = slot % 3;
            return s == 0 ? glm::dvec3(spacing, 0.0, spacing * 0.6)
                 : s == 1 ? glm::dvec3(-spacing, 0.0, spacing * 0.6)
                          : glm::dvec3(0.0, 0.0, spacing * 1.2 * (1 + slot / 3));
        }
        case FormationKind::Line:
            return glm::dvec3(side * rank * spacing, 0.0, 0.0);
    }
    return glm::dvec3(0.0, 0.0, spacing);
}

struct FormationInput
{
    glm::dvec3 leaderPosition {0.0};
    glm::dvec3 leaderVelocity {0.0};
    glm::dvec3 leaderForward {0.0, 0.0, -1.0};
    glm::dvec3 leaderUp {0.0, 1.0, 0.0};
    double leaderLengthMeters = 30.0;
    glm::dvec3 position {0.0};
    glm::dvec3 velocity {0.0};
    double lengthMeters = 30.0;
    FormationKind kind = FormationKind::Wedge;
    int slot = 0;
    double comfortAccelMps2 = 4.0;     // the approach brakes no harder than this (relative to the leader)
    double maxClosingMps = 60.0;       // the fastest approach relative to the leader
};

struct FormationReference
{
    FormationPhase phase = FormationPhase::Wide;
    glm::dvec3 aim {0.0};              // the point the phase flies to
    glm::dvec3 slot {0.0};             // the slot itself
    glm::dvec3 desiredVelocity {0.0};  // the leader's velocity + the closing
    glm::dvec3 desiredForward {0.0, 0.0, -1.0};
    glm::dvec3 desiredUp {0.0, 1.0, 0.0};
    double distanceToAim = 0.0;
    double distanceToSlot = 0.0;
    double spacing = 0.0;
};

// the phase moves on (never back except Hold -> Close when knocked far out of the slot)
inline FormationReference formationReference(const FormationInput& in, FormationPhase phase)
{
    FormationReference out;
    glm::dvec3 fwd = glm::length(in.leaderForward) > 1.0e-9 ? glm::normalize(in.leaderForward) : glm::dvec3(0, 0, -1);
    glm::dvec3 up = in.leaderUp - fwd * glm::dot(in.leaderUp, fwd);
    up = glm::length(up) > 1.0e-9 ? glm::normalize(up) : glm::dvec3(0, 1, 0);
    const glm::dvec3 right = glm::normalize(glm::cross(fwd, up));
    out.spacing = 2.5 * std::max(in.leaderLengthMeters, in.lengthMeters) + 20.0;
    const glm::dvec3 off = formationSlotOffset(in.kind, in.slot, out.spacing);
    out.slot = in.leaderPosition + right * off.x + up * off.y - fwd * off.z;
    const double sideSign = off.x >= 0.0 ? 1.0 : -1.0;
    const glm::dvec3 behind = in.leaderPosition - fwd * (6.0 * out.spacing) + right * (off.x + sideSign * out.spacing);
    const glm::dvec3 wide = in.leaderPosition + right * sideSign * (4.0 * out.spacing);
    const glm::dvec3 rel = in.position - in.leaderPosition;
    const bool ahead = glm::dot(rel, fwd) > -out.spacing;
    out.distanceToSlot = glm::length(out.slot - in.position);

    // the phase
    out.phase = phase;
    if (out.phase == FormationPhase::Wide && !ahead)
        out.phase = FormationPhase::Behind;
    if (out.phase == FormationPhase::Wide && std::abs(glm::dot(rel, right)) > 3.0 * out.spacing)
        out.phase = FormationPhase::Behind;   // out to the side: now round behind
    if (out.phase == FormationPhase::Behind && glm::length(behind - in.position) < 0.5 * out.spacing)
        out.phase = FormationPhase::Close;
    if (out.phase == FormationPhase::Close && out.distanceToSlot < 0.15 * out.spacing &&
        glm::length(in.velocity - in.leaderVelocity) < 3.0)
        out.phase = FormationPhase::Hold;
    if (out.phase == FormationPhase::Hold && out.distanceToSlot > 1.5 * out.spacing)
        out.phase = FormationPhase::Close;    // knocked out of the slot: close in again

    out.aim = out.phase == FormationPhase::Wide ? wide : out.phase == FormationPhase::Behind ? behind : out.slot;
    const glm::dvec3 d = out.aim - in.position;
    out.distanceToAim = glm::length(d);
    // the closing speed relative to the leader: no faster than the comfort braking allows to stop at the aim
    const double close = std::min({in.maxClosingMps, std::sqrt(2.0 * in.comfortAccelMps2 * std::max(0.0, out.distanceToAim - 2.0)),
                                   0.5 * out.distanceToAim});
    const glm::dvec3 dir = out.distanceToAim > 1.0e-6 ? d / out.distanceToAim : glm::dvec3(0.0);
    out.desiredVelocity = in.leaderVelocity + dir * close;
    // the course: along the motion while it moves, the leader's attitude in the slot / when (nearly) at rest
    const double vlen = glm::length(out.desiredVelocity);
    out.desiredForward = out.phase == FormationPhase::Hold || vlen < 3.0 ? fwd : out.desiredVelocity / vlen;
    out.desiredUp = up;
    return out;
}

} // namespace game::navigation::autopilot
