#pragma once

// Construction plans of the hub cities (author 2026-11-19: строящиеся хабы из строительных модулей разной готовности,
// план расширения готовых хабов и его реализация во времени, остановки / заморозки строек). The plan of a hub is
// assets/data/navigation/hub_expansion/<hub>.json (tools/models/hub_expansion.py, from the world site's construction /
// expansion of tools/colonization_lab/hub_construction.py): projects with a start day, working days and halts; the
// city at a date is a pure function of the plan and the game day (UniverseClock: 1 game day = 1 real day), evaluated
// once per day and cached. HubCityAssembly applies it to the frozen layout (a layout node of a hub being built shows
// its construction stage; an expansion adds its deck, its building in stages and its temporary props); the
// construction traffic (src/scene/HubConstructionTraffic.h) reads the projects' state and sites.

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace elite::model_asset
{
enum class ConstructionStatus : int
{
    Planned = 0,
    Building,
    Halted,
    Abandoned,
    Done
};

struct ExpansionNode
{
    std::string id;
    std::string folder;
    glm::vec3 position {0.0f};
    glm::vec3 rotationDeg {0.0f};
};

struct ExpansionProject
{
    std::string id;
    std::string kind;              // "build" (the hub itself) | tower | hab | slab | hall | dock | yard | mast | minehead
    int startDay = 0;
    int workDays = 1;
    struct Halt { int from = 0; int to = -1; };   // to < 0: abandoned for good
    std::vector<Halt> halts;
    struct Replace { std::uint32_t node = 0; int kit = 0; float a = 0.0f, b = 1.0f; };
    std::vector<Replace> replace;
    struct Add { ExpansionNode node; int kit = 0; float a = 0.0f, b = 1.0f; };
    std::vector<Add> add;
    std::vector<ExpansionNode> fixed;   // deck plates / spoke girders: from the start on, kept
    std::vector<ExpansionNode> props;   // jigs, temporary decks, barracks, stacks: while under way / halted / abandoned
    std::vector<glm::vec4> platforms;   // x, y (deck top), z, yaw deg
    std::vector<glm::vec3> front;       // build-front points
};

struct HubExpansionPlan
{
    std::string hubId;
    double epochUniverseS = 853005600.0;
    double daySeconds = 86400.0;
    bool gravity = false;
    std::vector<std::vector<std::string>> kits;   // kit index -> stage folders
    std::vector<std::string> kitNames;
    std::vector<ExpansionProject> projects;
};

// the plan of `hubId` (cached per process; nullptr: none)
const HubExpansionPlan* hubExpansionPlan(const std::string& hubId);

// TRADE step 16 (2026-11-20): a player's construction contract takes over a PLANNED project of a hub - its start, work
// days and halts are replaced at runtime (the project's own geometry / stage kits are drawn as before, no new meshes);
// the stage keys are recomputed at once. false = no such hub plan / project. Same process only (no replication yet).
bool setHubExpansionProjectOverride(const std::string& hubId, const std::string& projectId, int startDay, int workDays,
                                    const std::vector<ExpansionProject::Halt>& halts);

// the runtime overrides in force (the server replicates them; a client applies them with the setter above) - audit
// SRV-1: a contract changed the plan in the server's process only, a remote client drew the original plan
struct HubExpansionOverride
{
    std::string hubId;
    std::string projectId;
    int startDay = 0;
    int workDays = 1;
    std::vector<ExpansionProject::Halt> halts;
};
std::vector<HubExpansionOverride> hubExpansionOverrides();
std::uint64_t hubExpansionOverrideGeneration();

// THE HUB CONTENT FINGERPRINT (audit SRV-1): the hub city layouts and construction plans (their bytes) and the hub
// module packages (their paths and sizes) the geometry is assembled from on each side - a client whose geometry would
// differ from the server's is refused at the welcome instead of meeting walls it does not see. Computed once (~35 MB
// read), 0 when nothing is found.
std::uint64_t hubContentFingerprint();

// the game day of a universe time (day 0 = 3126-01-01)
int hubExpansionDay(const HubExpansionPlan& plan, double universeSeconds);

// progress 0..1 and status of a project on a day
float projectProgress(const ExpansionProject& p, int day, ConstructionStatus* status = nullptr);

// the stage folder of a kit at the node's u = (progress - a) / (b - a); empty = the finished module
const std::string& kitStageFolder(const HubExpansionPlan& plan, int kit, float u);

// the universe time the city assembly uses (set by the client each frame from its clock; before that: the real
// clock's universe time, UniverseClock::reset)
void setHubExpansionUniverseTime(double universeSeconds);
double hubExpansionUniverseTime();

// a key of the stages every node of `hubId` shows now (0: no plan) - cheap (cached per hub per day); a changed key
// means the city must be re-assembled
std::uint64_t hubExpansionStageKey(const std::string& hubId);

} // namespace elite::model_asset
