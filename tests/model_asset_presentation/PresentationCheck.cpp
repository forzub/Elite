// Runtime presentation check (pure CPU, no GL):
//   1. far ship ladder POINT -> PROXY -> SHELL -> COMPONENT -> FULL, no thrash
//   2. damaged ship: intact hull hides interiors; breach shows ONE compartment;
//      opening the hatch adds the next; nothing else
//   3. structural detach: broken links -> separate island (own object)
//   4. atomic state switch: collision / hit / opening / repair / visual
//   5. cost table (draw calls, triangles, CPU select time)
//   6. optional real asset (station): per-component LOD with the camera at
//      one end of the station
//
//   EliteModelAssetPresentationCheck [production.elmodel] [report.txt]

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetLodSelection.h"
#include "src/model_asset/ModelAssetPresentation.h"

using namespace elite::model_asset;
namespace mp = elite::model_asset::presentation;

namespace
{
int g_failures = 0;
std::FILE* g_report = nullptr;

void out(const char* fmt, const std::string& s)
{
    std::printf(fmt, s.c_str());
    if (g_report) std::fprintf(g_report, fmt, s.c_str());
}
void line(const std::string& s) { out("%s\n", s); }
void check(bool ok, const std::string& what)
{
    line(std::string(ok ? "[PASS] " : "[FAIL] ") + what);
    if (!ok) ++g_failures;
}
std::string fmt(const char* f, double a = 0, double b = 0, double c = 0, double d = 0)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), f, a, b, c, d);
    return buf;
}

MeshLod box(glm::vec3 half, int subdivisions)
{
    // A box split into (subdivisions^2 * 2 * 6) triangles - a stand-in mesh
    // whose triangle count scales with the LOD.
    MeshLod m;
    const int s = std::max(1, subdivisions);
    for (int face = 0; face < 6; ++face)
    {
        const int axis = face / 2;
        const float sign = (face % 2) ? 1.0f : -1.0f;
        const auto base = static_cast<std::uint32_t>(m.vertices.size());
        for (int j = 0; j <= s; ++j)
            for (int i = 0; i <= s; ++i)
            {
                glm::vec3 p(0.0f);
                const float u = -1.0f + 2.0f * i / s, v = -1.0f + 2.0f * j / s;
                p[axis] = sign;
                p[(axis + 1) % 3] = u;
                p[(axis + 2) % 3] = v;
                Vertex vx;
                vx.position = p * half;
                vx.normal = glm::vec3(0.0f);
                vx.normal[axis] = sign;
                m.vertices.push_back(vx);
            }
        for (int j = 0; j < s; ++j)
            for (int i = 0; i < s; ++i)
            {
                const std::uint32_t a = base + j * (s + 1) + i;
                m.triangles.push_back({a, a + 1, a + s + 2});
                m.triangles.push_back({a, a + s + 2, a + s + 1});
            }
    }
    m.minBounds = -half;
    m.maxBounds = half;
    return m;
}

std::int32_t addNode(ModelAsset& a, const std::string& id, const std::string& module)
{
    Node n;
    n.id = id;
    n.moduleId = module;
    a.nodes.push_back(n);
    return static_cast<std::int32_t>(a.nodes.size() - 1);
}

void addRender(RenderLod& lod, const std::string& id, std::int32_t node, glm::vec3 pos, glm::vec3 half, int sub,
               std::vector<std::string> states = {})
{
    RenderGeometryDefinition g;
    g.id = id;
    g.mesh = box(half, sub);
    lod.geometries.push_back(g);
    RenderNode rn;
    rn.id = id;
    rn.geometryIndex = static_cast<std::int32_t>(lod.geometries.size() - 1);
    rn.semanticNodeIndex = node;
    rn.localPosition = pos;
    rn.activeStates = std::move(states);
    lod.nodes.push_back(rn);
}

// A 22 m ship (Cobra-sized): hull fore/aft sections (aft breachable), two
// engines, an antenna, a cockpit canopy; interior compartments engineering
// (behind the aft hull) and cockpit, joined by a hatch.
ModelAsset makeShip()
{
    ModelAsset a;
    a.assetId = "check_ship";
    const auto hullF = addNode(a, "hull_fore", "hull");
    const auto hullA = addNode(a, "hull_aft", "hull");
    const auto engL = addNode(a, "engine_l", "propulsion");
    const auto engR = addNode(a, "engine_r", "propulsion");
    const auto antenna = addNode(a, "antenna", "sensors");
    const auto canopy = addNode(a, "canopy", "hull");
    const auto reactor = addNode(a, "reactor", "");      // interior: engineering
    const auto console = addNode(a, "console", "");      // interior: cockpit
    const auto hatch = addNode(a, "hatch", "");
    a.stateVariants.push_back({"breached", "Breached", hullA});
    a.stateVariants.push_back({"open", "Open", hatch});

    struct Level { RenderLodRole role; float rge; int sub; };
    const Level levels[] = {{RenderLodRole::Component, 0.0f, 24}, {RenderLodRole::Component, 0.004f, 8},
                            {RenderLodRole::Component, 0.02f, 3}, {RenderLodRole::ModuleProxy, 0.05f, 1},
                            {RenderLodRole::Shell, 0.08f, 2}, {RenderLodRole::Proxy, 0.25f, 1}};
    std::uint32_t level = 0;
    for (const Level& L : levels)
    {
        RenderLod lod;
        lod.level = level++;
        lod.role = L.role;
        lod.relativeGeometricError = L.rge;
        lod.sourceKind = "manual";
        if (L.role == RenderLodRole::Component)
        {
            addRender(lod, "hull_fore", hullF, {0, 0, -5.5f}, {6, 2.2f, 5.5f}, L.sub);
            addRender(lod, "hull_aft", hullA, {0, 0, 5.5f}, {6, 2.2f, 5.5f}, L.sub, {"intact"});
            // The breached replacement: same shell with a hole (stand-in).
            addRender(lod, "hull_aft_breached", hullA, {0, 0, 5.5f}, {6, 2.2f, 5.5f}, L.sub, {"breached"});
            addRender(lod, "engine_l", engL, {-3, 0, 11.6f}, {1.2f, 1.0f, 0.6f}, L.sub);
            addRender(lod, "engine_r", engR, {3, 0, 11.6f}, {1.2f, 1.0f, 0.6f}, L.sub);
            if (L.rge < 0.01f) // the antenna is dropped from LOD2 on (omitted feature)
                addRender(lod, "antenna", antenna, {0, 2.6f, 2}, {0.05f, 0.4f, 0.05f}, L.sub);
            addRender(lod, "canopy", canopy, {0, 2.4f, -6}, {1.4f, 0.4f, 2}, L.sub);
            addRender(lod, "reactor", reactor, {0, 0, 5}, {1.5f, 1.5f, 2}, L.sub);
            addRender(lod, "console", console, {0, 0, -7}, {1, 0.6f, 0.6f}, L.sub);
            addRender(lod, "hatch", hatch, {0, 0, 0}, {1, 1, 0.05f}, L.sub, {"intact"});
        }
        else if (L.role == RenderLodRole::ModuleProxy)
        {
            addRender(lod, "hull_proxy", hullF, {0, 0, 0}, {6, 2.4f, 11}, 1);
            addRender(lod, "propulsion_proxy", engL, {0, 0, 11.6f}, {4.2f, 1, 0.6f}, 1);
        }
        else
        {
            addRender(lod, L.role == RenderLodRole::Shell ? "shell" : "proxy", NoIndex, {0, 0, 0.3f},
                      {6, 2.4f, 11.6f}, L.sub);
        }
        a.renderLods.push_back(lod);
    }
    // Damage data scoped by the hull_aft state: switch atomically.
    CollisionVolume intactCol; intactCol.id = "col_aft"; intactCol.parentNodeIndex = hullA;
    intactCol.activeStates = {"intact"};
    CollisionVolume breachedCol = intactCol; breachedCol.id = "col_aft_breached"; breachedCol.activeStates = {"breached"};
    a.collisionVolumes = {intactCol, breachedCol};
    HitRegion hit; hit.id = "hit_aft"; hit.parentNodeIndex = hullA; hit.activeStates = {"intact"};
    a.hitRegions = {hit};
    Opening breach; breach.id = "breach_aft"; breach.parentNodeIndex = hullA; breach.activeStates = {"breached"};
    breach.localPosition = {0, 2.2f, 5.5f}; breach.halfSize = {1.5f, 1.5f, 0.2f};
    a.openings = {breach};
    RepairTarget repair; repair.id = "repair_aft"; repair.parentNodeIndex = hullA; repair.activeStates = {"breached"};
    repair.repairedStateId = "intact";
    a.repairTargets = {repair};
    // Compartments + portals: the breach looks UP (+Y) out of engineering.
    Compartment eng; eng.id = "engineering"; eng.role = "reactor"; eng.localPosition = {0, 0, 5.5f};
    eng.halfSize = {5, 2, 5}; eng.interiorNodeIndices = {reactor};
    Compartment cockpit; cockpit.id = "cockpit"; cockpit.role = "cockpit"; cockpit.localPosition = {0, 0, -5.5f};
    cockpit.halfSize = {5, 2, 5}; cockpit.interiorNodeIndices = {console};
    a.compartments = {eng, cockpit};
    CompartmentPortal breachPortal; breachPortal.id = "p_breach"; breachPortal.compartmentA = 0;
    breachPortal.compartmentB = NoIndex; breachPortal.parentNodeIndex = hullA; breachPortal.openingIndex = 0;
    breachPortal.localPosition = {0, 2.2f, 5.5f}; breachPortal.localRotationDeg = {-90, 0, 0}; // +Z -> +Y
    breachPortal.halfSize = {1.5f, 1.5f, 0.1f};
    CompartmentPortal hatchPortal; hatchPortal.id = "p_hatch"; hatchPortal.compartmentA = 0; hatchPortal.compartmentB = 1;
    hatchPortal.doorNodeIndex = hatch; hatchPortal.openStateId = "open"; hatchPortal.halfSize = {1, 1, 0.05f};
    a.portals = {breachPortal, hatchPortal};
    // Structure: fore-aft weld, engines mounted on aft, antenna on fore.
    const auto link = [&](const char* id, std::int32_t x, std::int32_t y) {
        StructuralLinkDefinition l; l.id = id; l.nodeAIndex = x; l.nodeBIndex = y; a.structuralLinks.push_back(l);
    };
    link("weld_hull", hullF, hullA);
    link("mount_engine_l", hullA, engL);
    link("mount_engine_r", hullA, engR);
    link("mount_antenna", hullF, antenna);
    link("mount_canopy", hullF, canopy);
    for (auto n : {reactor, hatch}) link(("int_" + a.nodes[static_cast<std::size_t>(n)].id).c_str(), hullA, n);
    link("int_console", hullF, console);
    for (auto& lod : a.renderLods)
    {
        bool has = false;
        for (auto& g : lod.geometries) { (void)g; has = true; }
        lod.minBounds = {-6, -2.4f, -11.6f};
        lod.maxBounds = {6, 2.8f, 12.2f};
        (void)has;
    }
    a.minBounds = {-6, -2.4f, -11.6f};
    a.maxBounds = {6, 2.8f, 12.2f};
    return a;
}

mp::View viewFrom(glm::vec3 eye, glm::vec3 target, float fovYDeg = 60.0f, float heightPx = 1080.0f)
{
    mp::View v;
    v.cameraAsset = eye;
    v.focalPx = heightPx / (2.0f * std::tan(glm::radians(fovYDeg) * 0.5f));
    const glm::mat4 proj = glm::perspective(glm::radians(fovYDeg), 16.0f / 9.0f, 0.1f, 1.0e7f);
    const glm::mat4 view = glm::lookAt(eye, target, std::abs(glm::normalize(target - eye).y) > 0.99f
                                                        ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0));
    v.planes = mp::frustumPlanes(proj * view);
    v.cullFrustum = true;
    return v;
}

void shipChecks()
{
    const ModelAsset ship = makeShip();
    std::string error;
    check(ModelAssetBinary::validate(ship, &error), "synthetic ship validates " + error);
    const mp::Index index = mp::buildIndex(ship);

    // 1. Ladder: approach from 40 km to 12 m and back, with +-1.5 % jitter
    //    on every step to provoke thrash at the boundaries.
    line("-- ladder: far ship approach (22 m ship, 60 deg / 1080 px) --");
    mp::Selection sel;
    mp::ObjectState state;
    std::vector<mp::Representation> sequence;
    int transitions = 0, jitterFlips = 0;
    mp::Representation last = mp::Representation::Culled;
    std::vector<std::pair<double, std::string>> firstSeen;
    const auto stepAt = [&](double d) {
        const glm::vec3 eye(0.0f, 8.0f, -static_cast<float>(d));
        const mp::Frame f = mp::select(ship, index, state, viewFrom(eye, glm::vec3(0.0f)), sel);
        return f;
    };
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i <= 400; ++i)
        {
            const double t = pass == 0 ? i / 400.0 : 1.0 - i / 400.0;
            const double d = std::exp(std::log(40000.0) * (1.0 - t) + std::log(12.0) * t);
            // Jitter cycle around d: the representation may move once (a real
            // boundary crossing) but must never come back within the cycle.
            std::vector<mp::Representation> cycle;
            mp::Frame f;
            for (double j : {1.0, 1.015, 0.985, 1.015, 0.985, 1.0})
            {
                f = stepAt(d * j);
                cycle.push_back(f.representation);
                if (f.representation != last)
                {
                    ++transitions;
                    firstSeen.emplace_back(d, std::string(pass == 0 ? "approach " : "retreat  ") +
                                                  mp::representationName(f.representation) +
                                                  fmt(" at %.0f m (%.1f px)", d * j, f.objectPx));
                    last = f.representation;
                }
            }
            for (std::size_t a = 0; a + 2 < cycle.size() + 0; ++a)
                for (std::size_t b = a + 1; b < cycle.size(); ++b)
                    if (cycle[b] != cycle[a])
                    {
                        for (std::size_t c = b + 1; c < cycle.size(); ++c)
                            if (cycle[c] == cycle[a]) { ++jitterFlips; break; }
                        break;
                    }
            sequence.push_back(last);
        }
    for (const auto& [d, text] : firstSeen) line("  " + text);
    bool sawAll[6] {};
    for (auto r : sequence) sawAll[static_cast<int>(r)] = true;
    check(sawAll[1] && sawAll[2] && sawAll[3] && sawAll[4] && sawAll[5],
          "ship passes POINT -> PROXY -> SHELL -> COMPONENT -> FULL");
    check(transitions <= 11, fmt("monotone ladder: %.0f transitions over approach + retreat (<= 11)", transitions));
    check(jitterFlips == 0, fmt("no thrash under +-1.5%% distance jitter (flips=%.0f)", jitterFlips));

    // 2. Damage / interiors at close range (FULL), camera above the aft hull.
    line("-- damaged ship: interiors through real openings --");
    const glm::vec3 above(0.0f, 22.0f, 3.0f), below(0.0f, -22.0f, 3.0f);
    const glm::vec3 aim(0.0f, 0.0f, -1.0f);
    mp::Selection s2;
    mp::ObjectState intact;
    mp::Frame f = mp::select(ship, index, intact, viewFrom(above, aim), s2);
    line(mp::formatFrame(ship, index, f, "ship_intact"));
    check(f.representation == mp::Representation::Full, "close view is FULL");
    check(f.visibleCompartments.empty() && f.interiorDraws == 0, "intact hull: no interior drawn from outside");
    const auto intactDraws = f.draws.size();
    const auto intactTris = f.triangles;

    mp::ObjectState breached;
    breached.nodeState.assign(ship.nodes.size(), "");
    breached.nodeState[1] = "breached";
    f = mp::select(ship, index, breached, viewFrom(above, aim), s2);
    line(mp::formatFrame(ship, index, f, "ship_breached"));
    check(f.visibleCompartments.size() == 1 && f.visibleCompartments[0] == 0,
          "breach: only the compartment behind it (engineering) is visible");
    const auto oneDraws = f.draws.size();
    const auto oneTris = f.triangles;
    const auto oneInterior = f.interiorDraws;

    mp::ObjectState hatchOpen = breached;
    hatchOpen.nodeState[8] = "open";
    f = mp::select(ship, index, hatchOpen, viewFrom(above, aim), s2);
    line(mp::formatFrame(ship, index, f, "ship_breached_hatch_open"));
    check(f.visibleCompartments.size() == 2, "open hatch: the next compartment (cockpit) becomes visible");
    const auto twoDraws = f.draws.size();
    const auto twoTris = f.triangles;
    const auto twoInterior = f.interiorDraws;

    f = mp::select(ship, index, hatchOpen, viewFrom(below, aim), s2);
    check(f.visibleCompartments.empty(), "breach facing away from the camera: no interior");
    mp::Selection farSel;
    f = mp::select(ship, index, hatchOpen, viewFrom(glm::vec3(0, 300, 5.5f), glm::vec3(0, 0, 5.5f)), farSel);
    check(f.visibleCompartments.empty(), std::string("breach seen at ") + mp::representationName(f.representation) +
                                             fmt(" (%.0f px object): interiors stay off", f.objectPx));

    // 3. Structural detach.
    line("-- structural graph: detach --");
    std::vector<std::uint8_t> broken(ship.structuralLinks.size(), 0);
    auto islands = mp::structuralIslands(ship, broken, 0);
    bool allZero = true;
    for (auto v : islands) allZero = allZero && v == 0;
    check(allZero, "intact structure: one island");
    broken[1] = 1; // mount_engine_l
    islands = mp::structuralIslands(ship, broken, 0);
    check(islands[2] != 0 && islands[3] == 0 && islands[0] == 0, "mount_engine_l broken: engine_l is a detached island");
    mp::ObjectState parent;
    parent.nodeHidden.assign(ship.nodes.size(), 0);
    parent.nodeHidden[2] = 1;
    mp::ObjectState fragment;
    fragment.nodeHidden.assign(ship.nodes.size(), 1);
    fragment.nodeHidden[2] = 0;
    mp::Selection ps, fs;
    const glm::vec3 side(40, 6, 0);
    const mp::Frame pf = mp::select(ship, index, parent, viewFrom(side, glm::vec3(0)), ps);
    const mp::Frame ff = mp::select(ship, index, fragment, viewFrom(side, glm::vec3(0)), fs);
    bool parentHasEngine = false, fragmentOnlyEngine = !ff.draws.empty();
    for (const auto& d : pf.draws) parentHasEngine = parentHasEngine || d.component == 2;
    for (const auto& d : ff.draws) fragmentOnlyEngine = fragmentOnlyEngine && d.component == 2;
    check(!parentHasEngine && fragmentOnlyEngine,
          fmt("detached engine leaves the ship and is drawn as its own object (%.0f draws, own LOD)", ff.draws.size()));

    // 4. Atomic state switch.
    const mp::ActiveSets a0 = mp::resolveActiveSets(ship, intact);
    const mp::ActiveSets a1 = mp::resolveActiveSets(ship, breached);
    check(a0.collisionVolumes == std::vector<std::int32_t> {0} && a1.collisionVolumes == std::vector<std::int32_t> {1} &&
              a0.hitRegions.size() == 1 && a1.hitRegions.empty() && a0.openings.empty() && a1.openings.size() == 1 &&
              a0.repairTargets.empty() && a1.repairTargets.size() == 1,
          "hull_aft intact -> breached switches collision, hit, opening, repair together");

    // 4b. Moving part: LOD / culling follow where the part is NOW (relative
    //     camera <-> part position, whoever moves).
    line("-- moving part: LOD follows the current position --");
    {
        const glm::vec3 eye(40, 6, 0);
        const auto engineLod = [&](const mp::ObjectState& st, mp::Selection& s) {
            const mp::Frame fr = mp::select(ship, index, st, viewFrom(eye, glm::vec3(0)), s);
            for (const auto& c : fr.components)
                if (c.component == 2) return std::make_pair(c.lod, c.culled);
            return std::make_pair(std::int32_t(NoIndex), true);
        };
        mp::Selection rest, moved, offscreen;
        mp::ObjectState away;
        away.nodeMotion.assign(ship.nodes.size(), glm::mat4(1.0f));
        // Same direction as seen from the camera, 600 m further away.
        away.nodeMotion[2] = glm::translate(glm::mat4(1.0f), -glm::normalize(eye) * 600.0f);
        mp::ObjectState behind = away;
        behind.nodeMotion[2] = glm::translate(glm::mat4(1.0f), glm::normalize(eye) * 80.0f);
        const auto r0 = engineLod(intact, rest);
        const auto r1 = engineLod(away, moved);
        const auto r2 = engineLod(behind, offscreen);
        check(!r0.second && (r1.second || r1.first > r0.first),
              fmt("engine moved 600 m away: LOD %.0f -> %.0f (or culled)", r0.first, r1.first));
        check(r2.second, "engine moved behind the camera: culled by the frustum at its current position");
    }

    // 4c. Measured per-node error overrides the LOD-wide value.
    {
        ModelAsset measured = ship;
        // engine_l: every coarser LOD measured as visibly wrong (1 km).
        for (std::size_t l = 1; l < measured.renderLods.size(); ++l)
        {
            measured.renderLods[l].nodeGeometricErrorM.assign(measured.nodes.size(), -1.0f);
            measured.renderLods[l].nodeGeometricErrorM[2] = 1000.0f;
        }
        const mp::Index mi = mp::buildIndex(measured);
        mp::Selection a, b;
        const glm::vec3 eye(40, 6, 0);
        const mp::Frame fa = mp::select(ship, index, intact, viewFrom(eye, glm::vec3(0)), a);
        const mp::Frame fb = mp::select(measured, mi, intact, viewFrom(eye, glm::vec3(0)), b);
        std::int32_t la = NoIndex, lb = NoIndex;
        for (const auto& c : fa.components) if (c.component == 2) la = c.lod;
        for (const auto& c : fb.components) if (c.component == 2) lb = c.lod;
        check(la >= 1 && lb == 0, fmt("measured per-node error wins: engine LOD %.0f (LOD-wide) -> %.0f (measured)", la, lb));
    }

    // 5. Cost table.
    line("-- cost (synthetic ship, close FULL view; CPU = select() average) --");
    const auto timeSelect = [&](const mp::ObjectState& st, glm::vec3 eye) {
        mp::Selection s;
        const auto t0 = std::chrono::steady_clock::now();
        mp::Frame fr;
        for (int i = 0; i < 2000; ++i) fr = mp::select(ship, index, st, viewFrom(eye, aim), s);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 2000.0;
        return std::make_pair(fr, us);
    };
    // Whole object at LOD0 (what the legacy path does near the ship).
    std::size_t wholeDraws = 0, wholeTris = 0;
    for (const auto& c : index.components)
        if (!c.interior)
            for (auto rn : c.renderNodes[0])
                if (ship.renderLods[0].nodes[static_cast<std::size_t>(rn)].activeStates.empty() ||
                    ship.renderLods[0].nodes[static_cast<std::size_t>(rn)].activeStates[0] == "intact")
                { ++wholeDraws; wholeTris += index.renderNodes[0][static_cast<std::size_t>(rn)].triangles; }
    line(fmt("  whole-object LOD0 (legacy)       draws=%.0f tris=%.0f", wholeDraws, wholeTris));
    const auto mid = timeSelect(intact, glm::vec3(0, 30, 120));
    line(fmt("  component LOD (+HLOD) at 120 m  draws=%.0f tris=%.0f cpu=%.2f us  rep=", mid.first.draws.size(),
             mid.first.triangles, mid.second) + mp::representationName(mid.first.representation));
    const auto close = timeSelect(intact, above);
    line(fmt("  damaged OFF (intact), close     draws=%.0f tris=%.0f cpu=%.2f us", close.first.draws.size(),
             close.first.triangles, close.second));
    line(fmt("  one visible compartment         draws=%.0f tris=%.0f interior draws=%.0f", oneDraws, oneTris, oneInterior));
    line(fmt("  two visible compartments        draws=%.0f tris=%.0f interior draws=%.0f", twoDraws, twoTris, twoInterior));
    (void)intactDraws;
    (void)intactTris;
}

void assetChecks(const std::string& path)
{
    ModelAsset asset;
    std::string error;
    if (!ModelAssetBinary::load(path, asset, &error))
    {
        check(false, "load " + path + ": " + error);
        return;
    }
    const mp::Index index = mp::buildIndex(asset);
    const glm::vec3 size = index.maxBounds - index.minBounds;
    line("-- asset " + path + " --");
    line(fmt("  bounds %.0f x %.0f x %.0f m, characteristic %.0f m", size.x, size.y, size.z, index.characteristicM));
    std::size_t lod0Tris = 0;
    for (const auto& info : index.renderNodes[0]) lod0Tris += info.drawable ? info.triangles : 0;
    line(fmt("  components=%.0f component LODs=%.0f  LOD0 total tris=%.0f", index.components.size(),
             index.componentLods.size(), lod0Tris));

    // Long axis of the station.
    int axis = 0;
    for (int k = 1; k < 3; ++k) if (size[k] > size[axis]) axis = k;
    const glm::vec3 centre = 0.5f * (index.minBounds + index.maxBounds);
    struct Shot { const char* name; float along; float off; };
    const Shot shots[] = {{"camera 60 m off one END of the station", 0.5f, 60.0f},
                          {"camera 25 km off one END, along the long axis", 0.5f, 25000.0f},
                          {"camera 2 km from the station (side)", 0.0f, 2000.0f},
                          {"camera 30 km away", 0.0f, 30000.0f}};
    for (const Shot& s : shots)
    {
        glm::vec3 eye = centre, target = centre;
        eye[axis] = s.along > 0.0f ? index.maxBounds[axis] + s.off : centre[axis];
        if (s.along <= 0.0f) eye[(axis + 1) % 3] = index.maxBounds[(axis + 1) % 3] + s.off;
        mp::Selection sel;
        mp::ObjectState st;
        const mp::View v = viewFrom(eye, target);
        mp::Frame f;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 50; ++i) f = mp::select(asset, index, st, v, sel);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 50.0;
        line(std::string("  ") + s.name + ":");
        line(mp::formatFrame(asset, index, f, asset.assetId));
        line(fmt("  select cpu=%.1f us  tris %.0f of LOD0 %.0f (%.1f %%)", us, f.triangles, lod0Tris,
                 100.0 * f.triangles / std::max<std::size_t>(1, lod0Tris)));
        if (s.along > 0.0f)
        {
            // Every drawn component meets the SSE target at ITS distance; the
            // nearest component is never coarser than the farthest one.
            float nearD = 1e30f, farD = 0.0f, worstErr = 0.0f;
            std::int32_t nearLod = -1, farLod = -1;
            int culled = 0;
            for (const auto& c : f.components)
            {
                if (c.culled) { ++culled; continue; }
                const auto& comp = index.components[static_cast<std::size_t>(c.component)];
                const float d = glm::length(0.5f * (comp.minBounds + comp.maxBounds) - eye);
                if (d < nearD) { nearD = d; nearLod = c.lod; }
                if (d > farD) { farD = d; farLod = c.lod; }
                worstErr = std::max(worstErr, c.errorPx);
            }
            check(nearLod >= 0 && farLod >= nearLod && worstErr <= 1.8f + 1e-3f,
                  fmt("per-component LOD: nearest LOD%.0f (%.0f m), farthest LOD%.0f (%.0f m)", nearLod, nearD, farLod,
                      farD) + fmt(", worst error %.2f px", worstErr));
            line(fmt("  culled components at this view: %.0f", culled));
        }
    }
}
} // namespace

int main(int argc, char** argv)
{
    const std::string report = argc > 2 ? argv[2] : "";
    if (!report.empty()) g_report = std::fopen(report.c_str(), "w");
    shipChecks();
    if (argc > 1 && std::string(argv[1]) != "-") assetChecks(argv[1]);
    line(fmt("RESULT failures=%.0f", g_failures));
    if (g_report) std::fclose(g_report);
    return g_failures == 0 ? 0 : 1;
}
