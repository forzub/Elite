#pragma once

// The RING hub's bend (tools/models/units/hub_ring.py; author 2026-11-22: «закольцованный хаб ... верх будет иметь свою
// ориентацию»): the hub plan is the flat STRIP (x along the ring, y up from the deck, z across); the city frame is the
// ring - its axis = the frame's +Z through the origin, the buildings on the inner side:
//     theta = 2 pi (x - x0) / L,  r = R - y,  frame = (r sin theta, -r cos theta, z),  up = (-sin theta, cos theta, 0)
// A flat hub has no bend (valid false): every call is the identity. Everything the game takes from the plan in strip
// metres (pads, their capture zones, the pad service's way, the street traffic) goes through here into the frame.
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation
{
struct HubRingBend
{
    bool valid = false;
    double radius = 0.0;   // R: the deck's radius
    double length = 0.0;   // L: the strip's length = the circumference at the deck
    double x0 = 0.0;       // the strip x at theta 0 (the seam)
    double periodSeconds = 0.0;   // the spin of the ring (and its city) - 0: not turning
    // the SYNC GATE (tools/models/units/hub_ring.py): a frame point on the spin axis past the strip's open end - still
    // both in space and in the turning city; a rim pad's autopilot route ends there with the hull rolling with the ring
    bool hasSyncGate = false;
    glm::dvec3 syncGate {0.0};

    double thetaOf(double x) const { return 2.0 * 3.14159265358979323846 * (x - x0) / length; }

    glm::dvec3 toFrame(const glm::dvec3& s) const
    {
        if (!valid)
            return s;
        const double th = thetaOf(s.x), r = radius - s.y;
        return {r * std::sin(th), -r * std::cos(th), s.z};
    }
    glm::dvec3 toStrip(const glm::dvec3& f) const
    {
        if (!valid)
            return f;
        double th = std::atan2(f.x, -f.y);
        if (th < 0.0)
            th += 2.0 * 3.14159265358979323846;
        return {x0 + th * length / (2.0 * 3.14159265358979323846), radius - std::hypot(f.x, f.y), f.z};
    }
    // the strip's axes at x, in the frame: columns = strip +X (along the ring), +Y (up, to the axis), +Z (across)
    glm::dmat3 basisAt(double x) const
    {
        if (!valid)
            return glm::dmat3(1.0);
        const double th = thetaOf(x), c = std::cos(th), s = std::sin(th);
        return glm::dmat3(glm::dvec3(c, s, 0.0), glm::dvec3(-s, c, 0.0), glm::dvec3(0.0, 0.0, 1.0));
    }
    glm::dvec3 upAt(const glm::dvec3& stripPoint) const { return basisAt(stripPoint.x)[1]; }
    // a strip direction / velocity at a strip point -> the frame (the along-ring part scaled by r / R)
    glm::dvec3 vectorToFrame(const glm::dvec3& stripPoint, const glm::dvec3& v) const
    {
        if (!valid)
            return v;
        const glm::dmat3 B = basisAt(stripPoint.x);
        return B[0] * (v.x * (radius - stripPoint.y) / radius) + B[1] * v.y + B[2] * v.z;
    }
};

// euler degrees as the layouts use them (R = Rx * Ry * Rz) <-> a matrix
inline glm::dmat3 hubRotationXYZ(const glm::dvec3& deg)
{
    const glm::dvec3 a = glm::radians(deg);
    const double cx = std::cos(a.x), sx = std::sin(a.x), cy = std::cos(a.y), sy = std::sin(a.y), cz = std::cos(a.z),
                 sz = std::sin(a.z);
    const glm::dmat3 Rx(1, 0, 0, 0, cx, sx, 0, -sx, cx);   // column-major
    const glm::dmat3 Ry(cy, 0, -sy, 0, 1, 0, sy, 0, cy);
    const glm::dmat3 Rz(cz, sz, 0, -sz, cz, 0, 0, 0, 1);
    return Rx * Ry * Rz;
}
inline glm::dvec3 hubEulerXYZ(const glm::dmat3& m)
{
    // m[col][row]: R02 = m[2][0] = sin b, R12 = m[2][1] = -sin a cos b, R22 = m[2][2], R01 = m[1][0], R00 = m[0][0]
    const double sb = glm::clamp(m[2][0], -1.0, 1.0);
    const double b = std::asin(sb);
    double a, c;
    if (std::abs(sb) < 0.9999)
    {
        a = std::atan2(-m[2][1], m[2][2]);
        c = std::atan2(-m[1][0], m[0][0]);
    }
    else
    {
        a = std::atan2(m[1][2], m[1][1]);
        c = 0.0;
    }
    return glm::degrees(glm::dvec3(a, b, c));
}
} // namespace game::navigation
