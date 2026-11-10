// Ship asset check (step 3, slices S1 + S2 offline acceptance; pure CPU, no GL):
// the compiled Cobrett-I asset (tools/ship_asset_assemble) through the runtime presentation
// (ModelAssetPresentation::select), camera close (FULL):
//   1. intact ship: no compartment and no interior draw from outside;
//   2. one cover plate torn off ("breached"): exactly its compartment is visible, interior draws are its modules only;
//   3. a reactor pad shot away: the propulsion bay under it, nothing else;
//   4. two plates torn off: both compartments;
//   5. a breach seen from below (portal faces up): no interior;
//   6. atomic state switch: the plate's collision main volume leaves, the opening appears;
//   7. cost table: draws / triangles intact vs breached.
//
//   EliteShipAssetCheck <ship.elmodel> [report.txt]

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetPresentation.h"

using namespace elite::model_asset;
namespace mp = elite::model_asset::presentation;

namespace
{
int g_failures = 0;
std::FILE* g_report = nullptr;

void line(const std::string& s)
{
    std::printf("%s\n", s.c_str());
    if (g_report) std::fprintf(g_report, "%s\n", s.c_str());
}
void check(bool ok, const std::string& what)
{
    line(std::string(ok ? "[PASS] " : "[FAIL] ") + what);
    if (!ok) ++g_failures;
}

mp::View viewFrom(glm::vec3 eye, glm::vec3 target)
{
    const float fovYDeg = 60.0f, heightPx = 1080.0f;
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

std::int32_t nodeIndex(const ModelAsset& a, const std::string& id)
{
    for (std::size_t i = 0; i < a.nodes.size(); ++i)
        if (a.nodes[i].id == id) return static_cast<std::int32_t>(i);
    return NoIndex;
}
std::int32_t compIndex(const ModelAsset& a, const std::string& id)
{
    for (std::size_t i = 0; i < a.compartments.size(); ++i)
        if (a.compartments[i].id == id) return static_cast<std::int32_t>(i);
    return NoIndex;
}

std::set<std::int32_t> interiorComponents(const mp::Frame& f)
{
    std::set<std::int32_t> out;
    for (const auto& d : f.draws)
        if (d.interior) out.insert(d.component);
    return out;
}

std::set<std::int32_t> interiorOf(const ModelAsset& a, std::int32_t comp)
{
    const auto& v = a.compartments[static_cast<std::size_t>(comp)].interiorNodeIndices;
    return {v.begin(), v.end()};
}

std::string names(const ModelAsset& a, const std::set<std::int32_t>& s)
{
    std::string out;
    for (const auto i : s) out += (out.empty() ? "" : ",") + a.nodes[static_cast<std::size_t>(i)].id;
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: EliteShipAssetCheck <ship.elmodel> [report.txt]\n");
        return 2;
    }
    if (argc > 2) g_report = std::fopen(argv[2], "w");
    ModelAsset ship;
    std::string error;
    check(ModelAssetBinary::load(argv[1], ship, &error), "load " + std::string(argv[1]) + " " + error);
    if (g_failures) return 1;
    check(ModelAssetBinary::validate(ship, &error), "asset validates " + error);
    const mp::Index index = mp::buildIndex(ship);
    {
        char b[200];
        std::snprintf(b, sizeof(b), "index bounds (%.2f %.2f %.2f) .. (%.2f %.2f %.2f), asset bounds (%.2f %.2f %.2f) .. (%.2f %.2f %.2f)",
                      index.minBounds.x, index.minBounds.y, index.minBounds.z, index.maxBounds.x, index.maxBounds.y, index.maxBounds.z,
                      ship.minBounds.x, ship.minBounds.y, ship.minBounds.z, ship.maxBounds.x, ship.maxBounds.y, ship.maxBounds.z);
        line(b);
    }
    line("nodes " + std::to_string(ship.nodes.size()) + ", compartments " + std::to_string(ship.compartments.size()) +
         ", portals " + std::to_string(ship.portals.size()) + ", LODs " + std::to_string(ship.renderLods.size()));

    const auto blank = [&]() { mp::ObjectState s; s.nodeState.assign(ship.nodes.size(), ""); return s; };
    const auto centre = [&](const std::string& comp) {
        return ship.compartments[static_cast<std::size_t>(compIndex(ship, comp))].localPosition;
    };

    // 1. intact
    const glm::vec3 core = centre("c_cover_6");
    const glm::vec3 above = core + glm::vec3(0.0f, 24.0f, 6.0f);
    mp::Selection sel;
    mp::Frame f = mp::select(ship, index, blank(), viewFrom(above, core), sel);
    line(mp::formatFrame(ship, index, f, "cobra_intact"));
    check(f.representation == mp::Representation::Full, "close view above the spine is FULL");
    check(f.visibleCompartments.empty() && f.interiorDraws == 0, "intact hull: no interior drawn from outside");
    const auto intactTris = f.triangles;
    const auto intactDraws = f.draws.size();

    // 2. one plate torn off (the jump-core compartment)
    auto st = blank();
    st.nodeState[static_cast<std::size_t>(nodeIndex(ship, "plate_cover_6"))] = "breached";
    f = mp::select(ship, index, st, viewFrom(above, core), sel);
    line(mp::formatFrame(ship, index, f, "cobra_plate6_breached"));
    const auto c6 = compIndex(ship, "c_cover_6");
    check(f.visibleCompartments.size() == 1 && f.visibleCompartments[0] == c6,
          "plate torn off: only its compartment (jump core) is visible");
    const auto drawn = interiorComponents(f);
    check(!drawn.empty() && drawn == interiorOf(ship, c6), "interior draws = the compartment's modules [" + names(ship, drawn) + "]");
    const auto oneTris = f.triangles;
    const auto oneDraws = f.draws.size();

    // 3. reactor pad shot away -> the propulsion bay under it
    auto sp = blank();
    sp.nodeState[static_cast<std::size_t>(nodeIndex(ship, "pad_eq_er5_l"))] = "breached";
    const glm::vec3 bay = centre("c_bay_l");
    mp::Selection selBay;
    f = mp::select(ship, index, sp, viewFrom(bay + glm::vec3(0.0f, 24.0f, 6.0f), bay), selBay);
    line(mp::formatFrame(ship, index, f, "cobra_pad_l_breached"));
    const auto cb = compIndex(ship, "c_bay_l");
    check(f.visibleCompartments.size() == 1 && f.visibleCompartments[0] == cb && interiorComponents(f) == interiorOf(ship, cb),
          "pad shot away: only the left propulsion bay (ER-5 + 2 CR-1) [" + names(ship, interiorComponents(f)) + "]");

    // 4. two plates
    auto s2 = st;
    s2.nodeState[static_cast<std::size_t>(nodeIndex(ship, "plate_cover_8"))] = "breached";
    const glm::vec3 mid = (core + centre("c_cover_8")) * 0.5f;
    mp::Selection selTwo;
    f = mp::select(ship, index, s2, viewFrom(mid + glm::vec3(0.0f, 26.0f, 4.0f), mid), selTwo);
    line(mp::formatFrame(ship, index, f, "cobra_two_plates"));
    const std::set<std::int32_t> two(f.visibleCompartments.begin(), f.visibleCompartments.end());
    check(two == std::set<std::int32_t>{c6, compIndex(ship, "c_cover_8")}, "two plates torn off: exactly their two compartments");
    const auto twoTris = f.triangles;

    // 5. from below the breach faces away
    mp::Selection selBelow;
    f = mp::select(ship, index, st, viewFrom(core + glm::vec3(0.0f, -24.0f, 6.0f), core), selBelow);
    check(f.visibleCompartments.empty() && f.interiorDraws == 0, "breach seen from below (facing away): no interior");

    // 6. atomic state switch
    const mp::ActiveSets a0 = mp::resolveActiveSets(ship, blank());
    const mp::ActiveSets a1 = mp::resolveActiveSets(ship, st);
    check(a1.collisionVolumes.size() + 1 == a0.collisionVolumes.size() && a0.openings.empty() && a1.openings.size() == 1,
          "plate intact -> breached: its main collision volume leaves, one opening appears (seams stay)");

    // 7. cost
    char buf[256];
    std::snprintf(buf, sizeof(buf), "cost: intact draws=%zu tris=%u | one plate draws=%zu tris=%u (+%u) | two plates tris=%u (+%u)",
                  intactDraws, intactTris, oneDraws, oneTris, oneTris - intactTris, twoTris, twoTris - intactTris);
    line(buf);
    line(g_failures ? "SHIP ASSET CHECK: FAIL" : "SHIP ASSET CHECK: PASS");
    if (g_report) std::fclose(g_report);
    return g_failures ? 1 : 0;
}
