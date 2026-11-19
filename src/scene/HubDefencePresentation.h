#pragma once

// The hub city's defence ring as the clients see it (docs/HUB_SECURITY.md): the installations of
// game::traffic::security::buildDefenceLayout (bases + slewing heads), the stray bodies drifting in, the shots
// (laser beams, coilgun slugs), the impact flashes and the fragments - a pure function of the universe time and the
// hub plan (the server computes the same engagements and logs them), plus the turrets' lock on the ships the server
// put under fire-control lock (replicated motion.securityLevel 3). Models: assets/compiled/hub_modules/hubd_*
// (tools/models/units/hub_defence.py).
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{
class HubPlan;
}
namespace game::navigation::traffic
{
class HubLaneNetwork;
}

namespace scene
{
enum class HubDefenceModel : int
{
    PdlBase = 0,
    PdlHead,
    CoilBase,
    CoilHead,
    Vls,
    RadarMast,
    RadarDish,
    Picket,
    Rock,
    Beam,
    Flash,
    Count
};
const char* hubDefenceModelId(HubDefenceModel model);

struct HubDefenceInstance
{
    HubDefenceModel model = HubDefenceModel::PdlBase;
    glm::mat4 local {1.0f};   // city frame (may carry a scale: the rocks, the beams, the flashes)
    float distance = 0.0f;
    float radius = 10.0f;     // for the view-frustum test
};

struct HubDefenceStats
{
    int sites = 0;
    int bodies = 0;
    int shots = 0;
    int locked = 0;
};

// lockedTargets: the city-frame positions of the ships under fire-control lock (the turrets track them)
void hubDefencePresentation(const game::navigation::HubPlan& plan, const game::navigation::traffic::HubLaneNetwork* lanes,
                            double timeSeconds, const glm::vec3& cameraLocal, float radius, const std::vector<glm::dvec3>& lockedTargets,
                            std::vector<HubDefenceInstance>& out, HubDefenceStats* stats = nullptr);
} // namespace scene
