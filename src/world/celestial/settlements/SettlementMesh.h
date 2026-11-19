#pragma once

// The geometry of a surface settlement (SurfaceSettlementLayout): a plain triangle mesh, no GPU,
// in body-fixed coordinates relative to the settlement's anchor (its centre point on the ground).
// The structures stand on the field's flattened footprints (SettledSurfaceField) and reach 1 m
// below the ground (no seam). Albedo > 1.5 marks an emissive vertex (lights): the flight mesh
// shader shows albedo - 2 unlit.

#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"
#include "src/world/celestial/surface/SurfaceField.h"

namespace world::celestial::settlements
{

struct SettlementVertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 albedo;
};

struct SettlementMesh
{
    glm::dvec3 anchorBody {0.0};   // body-fixed, from the body centre (m)
    std::vector<SettlementVertex> vertices;
    std::vector<std::uint32_t> indices;
};

namespace mesh_detail
{
constexpr double Pi = 3.14159265358979323846;

// a local frame on the ground: x right, y up, z along the structure's heading
struct Frame
{
    glm::dvec3 origin;   // relative to the anchor
    glm::dvec3 right, up, fwd;
    glm::vec3 point(double x, double y, double z) const { return glm::vec3(origin + right * x + up * y + fwd * z); }
    glm::vec3 dir(const glm::dvec3& v) const { return glm::vec3(glm::normalize(right * v.x + up * v.y + fwd * v.z)); }
};

struct Builder
{
    SettlementMesh& m;
    void tri(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& n, const glm::vec3& col)
    {
        const auto base = static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back({a, n, col});
        m.vertices.push_back({b, n, col});
        m.vertices.push_back({c, n, col});
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
    }
    void quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n, const glm::vec3& col)
    {
        tri(a, b, c, n, col);
        tri(a, c, d, n, col);
    }
    // an axis-aligned box in the frame (centre, half sizes)
    void box(const Frame& f, const glm::dvec3& c, const glm::dvec3& h, const glm::vec3& col)
    {
        const auto P = [&](double sx, double sy, double sz) { return f.point(c.x + sx * h.x, c.y + sy * h.y, c.z + sz * h.z); };
        quad(P(-1, 1, -1), P(1, 1, -1), P(1, 1, 1), P(-1, 1, 1), f.dir({0, 1, 0}), col);
        quad(P(-1, -1, -1), P(-1, -1, 1), P(1, -1, 1), P(1, -1, -1), f.dir({0, -1, 0}), col);
        quad(P(1, -1, -1), P(1, -1, 1), P(1, 1, 1), P(1, 1, -1), f.dir({1, 0, 0}), col);
        quad(P(-1, -1, -1), P(-1, 1, -1), P(-1, 1, 1), P(-1, -1, 1), f.dir({-1, 0, 0}), col);
        quad(P(-1, -1, 1), P(-1, 1, 1), P(1, 1, 1), P(1, -1, 1), f.dir({0, 0, 1}), col);
        quad(P(-1, -1, -1), P(1, -1, -1), P(1, 1, -1), P(-1, 1, -1), f.dir({0, 0, -1}), col);
    }
    // a vertical cylinder wall (y0..y1) and optionally its top disc
    void cylinder(const Frame& f, double r, double y0, double y1, int seg, const glm::vec3& col, bool top)
    {
        for (int i = 0; i < seg; ++i)
        {
            const double a0 = 2.0 * Pi * i / seg, a1 = 2.0 * Pi * (i + 1) / seg;
            const glm::dvec3 n0(std::cos(a0), 0, std::sin(a0)), n1(std::cos(a1), 0, std::sin(a1));
            const glm::dvec3 nm = glm::normalize(n0 + n1);
            quad(f.point(r * n0.x, y0, r * n0.z), f.point(r * n1.x, y0, r * n1.z), f.point(r * n1.x, y1, r * n1.z),
                 f.point(r * n0.x, y1, r * n0.z), f.dir(nm), col);
            if (top)
                tri(f.point(0, y1, 0), f.point(r * n1.x, y1, r * n1.z), f.point(r * n0.x, y1, r * n0.z), f.dir({0, 1, 0}), col);
        }
    }
    // a flat ring (annulus) at height y, facing up
    void ring(const Frame& f, double r0, double r1, double y, int seg, const glm::vec3& col)
    {
        for (int i = 0; i < seg; ++i)
        {
            const double a0 = 2.0 * Pi * i / seg, a1 = 2.0 * Pi * (i + 1) / seg;
            quad(f.point(r0 * std::cos(a0), y, r0 * std::sin(a0)), f.point(r0 * std::cos(a1), y, r0 * std::sin(a1)),
                 f.point(r1 * std::cos(a1), y, r1 * std::sin(a1)), f.point(r1 * std::cos(a0), y, r1 * std::sin(a0)), f.dir({0, 1, 0}),
                 col);
        }
    }
    // a spherical cap: footprint radius r, height h (smooth normals)
    void cap(const Frame& f, double r, double h, int seg, int rings, const glm::vec3& col)
    {
        const double rho = (r * r + h * h) / (2.0 * h);
        const double cy = h - rho;
        const double phiMax = std::asin(std::min(1.0, r / rho));
        const auto at = [&](double phi, double th, glm::dvec3& n) {
            n = glm::dvec3(std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th));
            return f.point(rho * n.x, cy + rho * n.y, rho * n.z);
        };
        const auto base = static_cast<std::uint32_t>(m.vertices.size());
        for (int j = 0; j <= rings; ++j)
            for (int i = 0; i <= seg; ++i)
            {
                glm::dvec3 n;
                const glm::vec3 p = at(phiMax * j / rings, 2.0 * Pi * i / seg, n);
                m.vertices.push_back({p, f.dir(n), col});
            }
        for (int j = 0; j < rings; ++j)
            for (int i = 0; i < seg; ++i)
            {
                const std::uint32_t a = base + j * (seg + 1) + i, b = a + 1, c = a + (seg + 1), d = c + 1;
                m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
            }
    }
    // a half-cylinder shell along z (an arch over the ground), length l, radius r; the front end
    // (+z) is a dark opening, the back end closed
    void arch(const Frame& f, double r, double l, int seg, const glm::vec3& col, const glm::vec3& opening)
    {
        for (int i = 0; i < seg; ++i)
        {
            const double a0 = Pi * i / seg, a1 = Pi * (i + 1) / seg;
            const glm::dvec3 n0(std::cos(a0), std::sin(a0), 0), n1(std::cos(a1), std::sin(a1), 0);
            const glm::dvec3 nm = glm::normalize(n0 + n1);
            quad(f.point(r * n0.x, r * n0.y - 1.0, -l * 0.5), f.point(r * n1.x, r * n1.y - 1.0, -l * 0.5),
                 f.point(r * n1.x, r * n1.y - 1.0, l * 0.5), f.point(r * n0.x, r * n0.y - 1.0, l * 0.5), f.dir(nm), col);
            // the ends: the opening (front) and the closed back
            tri(f.point(0, -1.0, l * 0.5), f.point(r * n0.x, r * n0.y - 1.0, l * 0.5), f.point(r * n1.x, r * n1.y - 1.0, l * 0.5),
                f.dir({0, 0, 1}), opening);
            tri(f.point(0, -1.0, -l * 0.5), f.point(r * n1.x, r * n1.y - 1.0, -l * 0.5), f.point(r * n0.x, r * n0.y - 1.0, -l * 0.5),
                f.dir({0, 0, -1}), col);
        }
        // the frame of the portal: a slightly larger ring of the arch at the front
        for (int i = 0; i < seg; ++i)
        {
            const double a0 = Pi * i / seg, a1 = Pi * (i + 1) / seg;
            const double r1 = r * 1.12;
            quad(f.point(r * std::cos(a0), r * std::sin(a0) - 1.0, l * 0.5 + 0.05),
                 f.point(r1 * std::cos(a0), r1 * std::sin(a0) - 1.0, l * 0.5 + 0.05),
                 f.point(r1 * std::cos(a1), r1 * std::sin(a1) - 1.0, l * 0.5 + 0.05),
                 f.point(r * std::cos(a1), r * std::sin(a1) - 1.0, l * 0.5 + 0.05), f.dir({0, 0, 1}), glm::vec3(0.55f, 0.52f, 0.45f));
        }
    }
};
} // namespace mesh_detail

// The mesh of one settlement on its body. `field` is the body's (settled) surface field.
inline SettlementMesh buildSettlementMesh(const SettlementLayout& layout, const surface::SurfaceField& field)
{
    using namespace mesh_detail;
    SettlementMesh out;
    const double radius = field.referenceRadiusM();
    const auto groundAt = [&](const glm::dvec3& d) { return field.sample(glm::normalize(d), 1.0).heightM; };
    out.anchorBody = glm::normalize(layout.centreDirection) * (radius + groundAt(layout.centreDirection));
    Builder b {out};
    const glm::vec3 concrete(0.30f, 0.30f, 0.31f), metal(0.62f, 0.63f, 0.66f), dome(0.80f, 0.81f, 0.83f), dark(0.015f, 0.015f, 0.02f);
    const glm::vec3 yellow(0.80f, 0.62f, 0.10f), white(0.90f, 0.90f, 0.88f), rock(0.22f, 0.21f, 0.20f);
    // emissive (albedo - 2 shown unlit)
    const glm::vec3 padLight(2.0f + 0.55f, 2.0f + 1.0f, 2.0f + 0.60f), warning(2.0f + 1.0f, 2.0f + 0.12f, 2.0f + 0.08f),
        portalLight(2.0f + 1.0f, 2.0f + 0.85f, 2.0f + 0.55f);
    // construction
    const glm::vec3 freshConcrete(0.55f, 0.55f, 0.53f), primer(0.42f, 0.17f, 0.10f), container(0.20f, 0.33f, 0.45f),
        facade(0.48f, 0.50f, 0.53f), net(0.85f, 0.38f, 0.06f), scaffold(0.55f, 0.52f, 0.40f), craneYellow(0.85f, 0.66f, 0.08f),
        hutGrey(0.46f, 0.48f, 0.45f), hutRust(0.47f, 0.31f, 0.20f), grating(0.32f, 0.33f, 0.34f), windowDark(0.05f, 0.06f, 0.07f);
    const glm::vec3 flood(2.0f + 0.9f, 2.0f + 0.95f, 2.0f + 1.0f), windowLit(2.0f + 1.0f, 2.0f + 0.74f, 2.0f + 0.42f);
    for (const auto& s : layout.structures)
    {
        const glm::dvec3 up = glm::normalize(s.bodyDirection);
        glm::dvec3 north = glm::dvec3(0.0, 1.0, 0.0) - up * up.y;
        if (glm::length(north) < 1.0e-9)
            north = glm::dvec3(1.0, 0.0, 0.0) - up * up.x;
        north = glm::normalize(north);
        const glm::dvec3 east = glm::normalize(glm::cross(north, up));
        Frame f;
        f.up = up;
        f.fwd = north * std::cos(s.headingRad) + east * std::sin(s.headingRad);
        f.right = glm::normalize(glm::cross(f.fwd, f.up));
        f.origin = up * (radius + groundAt(up)) - out.anchorBody;
        switch (s.kind)
        {
        case StructureKind::Pad:
        {
            const double r = s.radiusM;
            b.cylinder(f, r, -1.0, 0.4, 48, concrete, true);
            b.ring(f, r * 0.86, r * 0.93, 0.43, 48, yellow);
            // an "H" on the deck, along the heading
            b.box(f, {-r * 0.22, 0.44, 0.0}, {0.9, 0.03, r * 0.34}, white);
            b.box(f, {r * 0.22, 0.44, 0.0}, {0.9, 0.03, r * 0.34}, white);
            b.box(f, {0.0, 0.44, 0.0}, {r * 0.22, 0.03, 0.9}, white);
            // the pad number: s.number short bars at the head of the pad
            for (int i = 0; i < s.number; ++i)
                b.box(f, {(i - (s.number - 1) * 0.5) * 2.2, 0.44, r * 0.62}, {0.5, 0.03, 1.6}, white);
            // rim lights
            for (int i = 0; i < 12; ++i)
            {
                const double a = 2.0 * Pi * i / 12.0;
                b.box(f, {r * 0.97 * std::cos(a), 0.6, r * 0.97 * std::sin(a)}, {0.35, 0.2, 0.35}, padLight);
            }
            break;
        }
        case StructureKind::Entrance:
        {
            const double r = s.radiusM * 0.55;
            b.box(f, {0.0, -0.5, 0.0}, {s.radiusM * 0.75, 0.7, s.radiusM * 0.9}, concrete);   // the apron
            b.arch(f, r, s.radiusM * 1.3, 24, metal, dark);
            for (const double x : {-r * 0.85, r * 0.85})
                b.box(f, {x, 0.6, s.radiusM * 0.65 + 0.4}, {0.4, 0.4, 0.2}, portalLight);
            break;
        }
        case StructureKind::Dome:
        {
            b.cylinder(f, s.radiusM * 1.02, -1.0, 1.4, 40, metal, false);
            Frame lifted = f;
            lifted.origin += f.up * 1.4;
            b.cap(lifted, s.radiusM, s.heightM, 40, 12, dome);
            // an airlock toward the settlement's axis
            b.box(f, {0.0, 1.6, -s.radiusM - 1.5}, {2.0, 1.8, 2.0}, metal);
            break;
        }
        case StructureKind::Antenna:
        {
            const double h = s.heightM;
            b.box(f, {0.0, 0.0, 0.0}, {2.5, 0.6, 2.5}, concrete);
            b.box(f, {0.0, h * 0.5, 0.0}, {0.7, h * 0.5, 0.7}, metal);
            // the dish: a tilted disc near the top
            Frame dish = f;
            dish.origin += f.up * (h * 0.78) + f.fwd * 1.2;
            const double tilt = 0.6;
            dish.up = glm::normalize(f.up * std::cos(tilt) + f.fwd * std::sin(tilt));
            dish.fwd = glm::normalize(glm::cross(f.right, dish.up));
            b.cylinder(dish, 5.0, -0.15, 0.15, 24, metal, true);
            b.box(f, {0.0, h + 0.5, 0.0}, {0.45, 0.45, 0.45}, warning);
            break;
        }
        case StructureKind::RockPassage:
        {
            b.box(f, {-s.radiusM * 0.9, s.heightM * 0.45, 0.0}, {s.radiusM * 0.45, s.heightM * 0.5, s.radiusM * 0.7}, rock);
            b.box(f, {s.radiusM * 0.9, s.heightM * 0.45, 0.0}, {s.radiusM * 0.45, s.heightM * 0.5, s.radiusM * 0.7}, rock);
            b.arch(f, s.radiusM * 0.5, s.radiusM * 1.2, 16, metal, dark);
            for (const double x : {-s.radiusM * 0.42, s.radiusM * 0.42})
                b.box(f, {x, 0.6, s.radiusM * 0.6 + 0.4}, {0.4, 0.4, 0.2}, portalLight);
            break;
        }
        // construction (docs/HUB_CONSTRUCTION.md): a stopped site keeps its shape, its work lights are off
        case StructureKind::BuildFrame:
        {
            const double w = s.radiusM * 0.72, H = s.heightM;
            const double hb = H * (s.stage <= 1 ? 0.0 : s.stage == 2 ? 0.35 : s.stage == 3 ? 0.7 : s.stage == 4 ? 0.97 : 1.0);
            b.box(f, {0.0, 0.0, 0.0}, {w * 1.08, 0.9, w * 1.08}, freshConcrete);                  // the raft
            if (s.stage <= 1)
            {
                for (int i = -1; i <= 1; ++i)                                                     // starter stubs
                    for (int k = -1; k <= 1; ++k)
                        b.box(f, {i * w * 0.85, 2.4, k * w * 0.85}, {0.5, 1.6, 0.5}, primer);
                b.box(f, {w * 1.35, 1.2, 0.0}, {2.0, 1.2, w * 0.35}, primer);                    // a beam stack
                b.box(f, {-w * 1.35, 1.5, w * 0.3}, {3.0, 1.5, 1.3}, container);                   // a container
            }
            else
            {
                const double floorH = 4.0 * std::max(1.0, std::round(H / 40.0));               // a deck every few storeys
                const double clad = s.stage == 3 ? hb * 0.5 : s.stage == 4 ? hb * 0.9 : s.stage >= 5 ? hb : 0.0;
                if (s.stage < 5)
                {
                    for (int i = -1; i <= 1; ++i)                                                 // the column grid
                        for (int k = -1; k <= 1; ++k)
                            b.box(f, {i * w * 0.85, hb * 0.5, k * w * 0.85}, {0.45, hb * 0.5, 0.45}, primer);
                    b.box(f, {0.0, hb * 0.54, 0.0}, {w * 0.2, hb * 0.54, w * 0.2}, freshConcrete);   // the core runs ahead
                    for (double y = floorH; y <= hb + 0.1; y += floorH)                          // decks + edge beams
                    {
                        b.box(f, {0.0, y, 0.0}, {w * 0.9, 0.18, w * 0.9}, y < clad + floorH ? freshConcrete : primer);
                        if (y > clad && s.stage >= 3)
                            b.box(f, {0.0, y + 0.6, w * 0.9}, {w * 0.9, 0.6, 0.05}, net);          // edge protection
                    }
                }
                if (clad > 0.0)
                {
                    // the cladding: four faces up to `clad`; nearly done: the +z face still open near the top
                    const double cz = s.stage == 4 ? clad * 0.75 : clad;
                    b.box(f, {0.0, clad * 0.5, -w * 0.92}, {w * 0.92, clad * 0.5, 0.3}, facade);
                    b.box(f, {0.0, cz * 0.5, w * 0.92}, {w * 0.92, cz * 0.5, 0.3}, facade);
                    b.box(f, {-w * 0.92, clad * 0.5, 0.0}, {0.3, clad * 0.5, w * 0.92}, facade);
                    b.box(f, {w * 0.92, clad * 0.5, 0.0}, {0.3, clad * 0.5, w * 0.92}, facade);
                    const glm::vec3 win = s.stage >= 5 && !s.stopped ? windowLit : windowDark;
                    for (double y = 5.0; y < cz - 2.0; y += 8.0)                                 // window bands
                        for (const double sz : {-1.0, 1.0})
                            b.box(f, {0.0, y, sz * (w * 0.92 + 0.32)}, {w * 0.7, 0.9, 0.05}, win);
                    if (s.stage >= 5)
                        b.box(f, {0.0, hb + 1.2, 0.0}, {w * 0.6, 1.2, w * 0.6}, metal);           // roof plant
                }
                if (s.stage == 4)                                                                  // scaffold on the open face
                    b.box(f, {0.0, hb * 0.5, w * 0.92 + 1.6}, {w * 0.8, hb * 0.5, 0.9}, scaffold);
            }
            if (!s.stopped && s.stage < 5)
                for (const double x : {-w * 1.1, w * 1.1})                                       // work floodlights
                    b.box(f, {x, 6.0, -w * 1.1}, {0.6, 0.4, 0.6}, flood);
            break;
        }
        case StructureKind::Crane:
        {
            const double H = s.heightM;
            b.box(f, {0.0, 0.8, 0.0}, {3.2, 0.8, 3.2}, freshConcrete);                          // the ballast base
            for (const double x : {-1.1, 1.1})
                for (const double z : {-1.1, 1.1})
                    b.box(f, {x, H * 0.5, z}, {0.18, H * 0.5, 0.18}, craneYellow);              // the mast chords
            for (double y = 4.0; y < H; y += 5.0)                                                  // lattice rings
            {
                b.box(f, {0.0, y, -1.1}, {1.1, 0.08, 0.08}, craneYellow);
                b.box(f, {0.0, y, 1.1}, {1.1, 0.08, 0.08}, craneYellow);
                b.box(f, {-1.1, y, 0.0}, {0.08, 0.08, 1.1}, craneYellow);
                b.box(f, {1.1, y, 0.0}, {0.08, 0.08, 1.1}, craneYellow);
            }
            // the slewing part: f.fwd is the jib's bearing
            const double jib = std::max(30.0, H * 0.55);
            b.box(f, {0.0, H + 0.8, 0.0}, {1.5, 0.8, 1.5}, craneYellow);
            b.box(f, {0.0, H + 1.6 + 1.0, -1.8}, {0.9, 1.0, 1.0}, metal);                          // the cab
            b.box(f, {0.0, H + 1.6 + 3.5, 0.0}, {0.35, 3.5, 0.35}, craneYellow);                  // the tower head
            b.box(f, {0.0, H + 2.2, jib * 0.5}, {0.6, 0.6, jib * 0.5}, craneYellow);              // the jib
            b.box(f, {0.0, H + 2.2, -jib * 0.15}, {0.6, 0.5, jib * 0.15}, craneYellow);           // the counter-jib
            b.box(f, {0.0, H + 1.2, -jib * 0.27}, {1.2, 1.2, 1.0}, freshConcrete);               // counterweight
            const double trolley = jib * (0.4 + 0.4 * std::fmod(s.headingRad * 3.7, 1.0));
            const double hookY = H * (s.stopped ? 0.92 : 0.45);
            b.box(f, {0.0, (H + 1.6 + hookY) * 0.5, trolley}, {0.05, (H + 1.6 - hookY) * 0.5, 0.05}, metal);   // the rope
            b.box(f, {0.0, hookY, trolley}, {0.5, 0.6, 0.5}, craneYellow);                        // the hook block
            if (!s.stopped)
                b.box(f, {0.0, hookY - 1.6, trolley}, {3.0, 0.6, 0.6}, primer);                    // a beam on the hook
            b.box(f, {0.0, H + 8.8, 0.0}, {0.35, 0.35, 0.35}, warning);                            // aviation lights
            b.box(f, {0.0, H + 3.0, jib}, {0.35, 0.35, 0.35}, warning);
            break;
        }
        case StructureKind::Barracks:
        {
            // two levels of 12 container huts (6 x 2.6 x 2.9 m), corrugated, small windows, an open gallery and stairs
            const double L = s.radiusM, unit = L / 6.0;
            for (int lv = 0; lv < 2; ++lv)
                for (int u = 0; u < 12; ++u)
                {
                    const double z = -L + unit * (u + 0.5);
                    const glm::vec3 col = ((u * 7 + lv * 3 + s.number) % 3 == 0) ? hutRust : hutGrey;
                    b.box(f, {0.0, 1.45 + lv * 2.95, z}, {3.0, 1.42, unit * 0.48}, col);
                    const bool lit = !s.stopped && ((u * 5 + lv + s.number) % 3) != 0;
                    b.box(f, {3.02, 1.7 + lv * 2.95, z}, {0.04, 0.45, 0.55}, lit ? windowLit : windowDark);
                    if (lv == 1 && u % 3 == 0)
                        b.box(f, {0.0, 6.2, z}, {0.8, 0.35, 0.6}, metal);                         // roof AC boxes
                }
            b.box(f, {3.9, 2.9, 0.0}, {0.9, 0.08, L}, metal);                                     // the gallery
            b.box(f, {4.75, 3.45, 0.0}, {0.04, 0.5, L}, metal);                                    // its rail
            for (int k = 0; k < 4; ++k)                                                            // the stair at the end
                b.box(f, {3.9, 0.36 + k * 0.72, L + 1.0 + k * 0.9}, {0.6, 0.36, 0.45}, metal);
            if (!s.stopped)
                b.box(f, {4.75, 4.2, -L}, {0.25, 0.25, 0.25}, flood);
            break;
        }
        case StructureKind::TempPad:
        {
            const double r = s.radiusM, h = s.heightM;
            for (const double x : {-r * 0.85, r * 0.85})                                           // truss legs
                for (const double z : {-r * 0.6, r * 0.6})
                    b.box(f, {x, h * 0.5, z}, {0.5, h * 0.5, 0.5}, primer);
            b.box(f, {0.0, h, 0.0}, {r, 0.4, r * 0.7}, grating);                                   // the deck
            b.box(f, {0.0, h + 0.42, 0.0}, {r * 0.5, 0.03, 0.5}, yellow);                          // the box marking
            b.box(f, {0.0, h + 0.42, 0.0}, {0.5, 0.03, r * 0.4}, yellow);
            for (int k = 0; k < 6; ++k)                                                            // the ramp (steps)
                b.box(f, {0.0, h * (k + 0.5) / 6.0, -r * 0.7 - 2.5 - (5 - k) * 3.0}, {4.0, h * 0.5 / 6.0, 1.5}, grating);
            for (const double x : {-r * 0.9, r * 0.9})
                for (const double z : {-r * 0.55, 0.0, r * 0.55})
                    b.box(f, {x, h + 0.9, z}, {0.3, 0.5, 0.3}, yellow);                            // bollards
            b.box(f, {r * 0.5, h + 1.6, r * 0.3}, {3.0, 1.3, 1.3}, container);                     // a container on it
            if (!s.stopped)
                for (const double x : {-r, r})
                    b.box(f, {x, h + 6.0, r * 0.7}, {0.6, 0.4, 0.6}, flood);
            break;
        }
        }
    }
    return out;
}

} // namespace world::celestial::settlements
