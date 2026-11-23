// EliteRouteDraftLab - the route-draft net's dataset and its check (author 2026-11-23: «сделать нейронку и загнать ее
// на обучение ... оптимум по времени ... штрафы за столкновения и слишком размашистые обходы ... в лабиринте ...
// подключить физическое состояние корабля»).
//
//   dataset <out.bin> <samples> <threads> <maze share 0..1> [scene dumps...]
//       per sample: a scene (a real dumped optimizer request = the hub's obstacles in its planning frame, or a generated
//       MAZE: walls with corridors, floor and ceiling - no room for a wide detour), a sampled start / goal and a sampled
//       SHIP STATE (design speed, acceleration, braking, lateral and angular authority from a damaged to a strong ship);
//       the TEACHER = the continuous optimizer with the time-optimal weights and 3x the game's budget. Rows of float32:
//       features (RouteDraftInputs), the teacher's shape (RouteDraftOutputs), its travel time, valid.
//   eval <net.json> <samples> <threads> <maze share> [scene dumps...]
//       per sample: the GAME's optimizer (its weights, 160 iterations), the teacher, and the NET (its draft refined
//       briefly with the time-optimal weights; when the draft is infeasible the full search runs): valid, travel time
//       against the teacher's, wall time, evaluations, detour (path / chord), the draft's own feasibility.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "src/game/navigation/planner/RouteDraftIo.h"
#include "src/game/navigation/planner/RouteDraftNet.h"

namespace lab = game::navigation::planner::assisted::lab;
using game::navigation::planner::RouteDraftInputs;
using game::navigation::planner::RouteDraftOutputs;
using world::navigation::NavigationObstacle;
using world::navigation::NavigationObstacleShape;

namespace
{
double uni(std::mt19937_64& g, double a, double b) { return std::uniform_real_distribution<double>(a, b)(g); }

glm::dvec3 randomUnit(std::mt19937_64& g)
{
    std::normal_distribution<double> n(0.0, 1.0);
    glm::dvec3 v(n(g), n(g), n(g));
    return glm::length(v) > 1.0e-9 ? glm::normalize(v) : glm::dvec3(1, 0, 0);
}

// v turned by a random angle up to maxRad
glm::dvec3 perturb(std::mt19937_64& g, const glm::dvec3& v, double maxRad)
{
    const glm::dvec3 axis = glm::normalize(glm::cross(v, randomUnit(g)) + glm::dvec3(1.0e-9));
    const double a = uni(g, 0.0, maxRad);
    return glm::normalize(v * std::cos(a) + glm::cross(axis, v) * std::sin(a) + axis * glm::dot(axis, v) * (1 - std::cos(a)));
}

bool insideAny(const std::vector<NavigationObstacle>& obs, const glm::dvec3& p, double hull)
{
    for (const auto& o : obs)
    {
        if (o.shape == NavigationObstacleShape::Box)
        {
            const glm::dvec3 q = glm::transpose(o.localToWorldBasis) * (p - o.centerMeters);
            const glm::dvec3 h = o.halfExtentsMeters + glm::dvec3(hull + o.requiredClearanceMeters);
            if (std::abs(q.x) < h.x && std::abs(q.y) < h.y && std::abs(q.z) < h.z)
                return true;
        }
        else if (glm::length(p - o.centerMeters) < o.conservativeRadiusMeters() + hull + o.requiredClearanceMeters)
            return true;
    }
    return false;
}

NavigationObstacle box(const glm::dvec3& c, const glm::dvec3& half)
{
    NavigationObstacle o;
    o.shape = NavigationObstacleShape::Box;
    o.centerMeters = c;
    o.halfExtentsMeters = half;
    o.localToWorldBasis = glm::dmat3(1.0);
    o.requiredClearanceMeters = 5.0;
    return o;
}

// a MAZE in the XZ plane: N x N cells of `cell` metres, walls `wallT` thick and `height` tall, a floor and a ceiling;
// corridors by a randomized depth-first carve (+ a few extra openings, so there are alternatives)
struct Maze
{
    std::vector<NavigationObstacle> obstacles;
    int n = 0;
    double cell = 0.0;
    std::vector<int> wallH, wallV;   // 1 = a wall on the +Z / +X side of the cell
    // the corridor path between two cells (BFS) and its number of turns; -1: none
    int turnsBetween(int a, int b) const
    {
        std::vector<int> prev(n * n, -1);
        std::vector<int> queue {a};
        prev[a] = a;
        for (std::size_t q = 0; q < queue.size(); ++q)
        {
            const int c = queue[q], i = c / n, j = c % n;
            const auto go = [&](int d, bool open) {
                if (open && prev[d] < 0) { prev[d] = c; queue.push_back(d); }
            };
            if (i + 1 < n) go(c + n, !wallV[c]);
            if (i > 0) go(c - n, !wallV[c - n]);
            if (j + 1 < n) go(c + 1, !wallH[c]);
            if (j > 0) go(c - 1, !wallH[c - 1]);
        }
        if (prev[b] < 0)
            return -1;
        int turns = 0, lastStep = 0;
        for (int c = b; c != a; c = prev[c])
        {
            const int step = c - prev[c];
            if (lastStep != 0 && step != lastStep)
                ++turns;
            lastStep = step;
        }
        return turns;
    }
    glm::dvec3 cellCenter(int i, int j) const { return glm::dvec3((i + 0.5) * cell, 0.0, (j + 0.5) * cell); }
};

Maze makeMaze(std::mt19937_64& g)
{
    Maze m;
    m.n = 4 + static_cast<int>(g() % 4);
    m.cell = uni(g, 220.0, 600.0);
    const double wallT = 20.0, height = uni(g, 0.5, 1.0) * m.cell;
    const int n = m.n;
    // walls: h[i][j] = the wall on the +Z side of cell (i, j), v[i][j] = on the +X side
    std::vector<int> wallH(n * n, 1), wallV(n * n, 1), seen(n * n, 0);
    std::vector<std::pair<int, int>> stack {{0, 0}};
    seen[0] = 1;
    while (!stack.empty())
    {
        auto [i, j] = stack.back();
        std::vector<int> dirs;
        if (i + 1 < n && !seen[(i + 1) * n + j]) dirs.push_back(0);
        if (i > 0 && !seen[(i - 1) * n + j]) dirs.push_back(1);
        if (j + 1 < n && !seen[i * n + j + 1]) dirs.push_back(2);
        if (j > 0 && !seen[i * n + j - 1]) dirs.push_back(3);
        if (dirs.empty())
        {
            stack.pop_back();
            continue;
        }
        const int d = dirs[g() % dirs.size()];
        int a = i, b = j;
        if (d == 0) { wallV[i * n + j] = 0; ++a; }
        if (d == 1) { wallV[(i - 1) * n + j] = 0; --a; }
        if (d == 2) { wallH[i * n + j] = 0; ++b; }
        if (d == 3) { wallH[i * n + j - 1] = 0; --b; }
        seen[a * n + b] = 1;
        stack.push_back({a, b});
    }
    for (int k = 0; k < n; ++k)   // a few loops
    {
        const int i = static_cast<int>(g() % (n - 1)), j = static_cast<int>(g() % (n - 1));
        (g() % 2 ? wallV : wallH)[i * n + j] = 0;
    }
    m.wallH = wallH;
    m.wallV = wallV;
    const double half = 0.5 * m.cell;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            const glm::dvec3 c = m.cellCenter(i, j);
            if (wallV[i * n + j] && i + 1 < n)
                m.obstacles.push_back(box(c + glm::dvec3(half, 0, 0), glm::dvec3(wallT / 2, height / 2, half)));
            if (wallH[i * n + j] && j + 1 < n)
                m.obstacles.push_back(box(c + glm::dvec3(0, 0, half), glm::dvec3(half, height / 2, wallT / 2)));
        }
    const double size = n * m.cell;
    // the outer walls, the floor and the ceiling (no flying over the top)
    m.obstacles.push_back(box(glm::dvec3(-wallT / 2, 0, size / 2), glm::dvec3(wallT / 2, height / 2, size / 2)));
    m.obstacles.push_back(box(glm::dvec3(size + wallT / 2, 0, size / 2), glm::dvec3(wallT / 2, height / 2, size / 2)));
    m.obstacles.push_back(box(glm::dvec3(size / 2, 0, -wallT / 2), glm::dvec3(size / 2, height / 2, wallT / 2)));
    m.obstacles.push_back(box(glm::dvec3(size / 2, 0, size + wallT / 2), glm::dvec3(size / 2, height / 2, wallT / 2)));
    m.obstacles.push_back(box(glm::dvec3(size / 2, -height / 2 - wallT / 2, size / 2), glm::dvec3(size / 2, wallT / 2, size / 2)));
    m.obstacles.push_back(box(glm::dvec3(size / 2, height / 2 + wallT / 2, size / 2), glm::dvec3(size / 2, wallT / 2, size / 2)));
    return m;
}

// the ship's physical state: a reference ship scaled from damaged (0.25) to strong (1.5) per authority
void sampleShip(std::mt19937_64& g, lab::Request& r, double designBase)
{
    r.designSpeedMps = designBase * uni(g, 0.5, 1.5);
    r.acceleratingMps2 = 8.0 * uni(g, 0.25, 1.5);
    r.brakingMps2 = 15.0 * uni(g, 0.25, 1.5);
    r.maxLateralAccelerationMps2 = 17.6 * uni(g, 0.25, 1.5);
    r.maxAngularVelocityRadPerSecond = 0.6 * uni(g, 0.25, 1.5);
    r.maxAngularAccelerationRadPerSecond2 = 0.4 * uni(g, 0.25, 1.5);
    r.hullRadiusMeters = uni(g, 12.0, 40.0);
}

bool sampleRequest(std::mt19937_64& g, const std::vector<lab::Request>& scenes, double mazeShare, lab::Request& r)
{
    if (scenes.empty() || uni(g, 0.0, 1.0) < mazeShare)
    {
        const Maze m = makeMaze(g);
        r = lab::Request {};
        r.obstacles = m.obstacles;
        sampleShip(g, r, uni(g, 25.0, 90.0));
        // one free leg = at most two turns (the optimizer's cubic -> straight -> cubic); farther cells are the route
        // composer's business (several legs)
        const int a = static_cast<int>(g() % (m.n * m.n));
        int b = a;
        for (int tries = 0; tries < 60; ++tries)
        {
            const int c = static_cast<int>(g() % (m.n * m.n));
            const int t = c == a ? -1 : m.turnsBetween(a, c);
            if (t >= 0 && t <= 2)
            {
                b = c;
                break;
            }
        }
        if (b == a)
            return false;
        const glm::dvec3 axes[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
        r.start.positionMeters = m.cellCenter(a / m.n, a % m.n);
        r.goal.positionMeters = m.cellCenter(b / m.n, b % m.n);
        r.start.tangent = axes[g() % 4];
        r.goal.tangent = axes[g() % 4];
        r.protectedStartStraightMeters = uni(g, 0.0, 0.3) * m.cell;
        r.protectedGoalStraightMeters = uni(g, 0.0, 0.3) * m.cell;
    }
    else
    {
        r = scenes[g() % scenes.size()];
        sampleShip(g, r, std::max(30.0, r.designSpeedMps));
        r.start.positionMeters += randomUnit(g) * uni(g, 0.0, 2000.0);
        r.start.tangent = perturb(g, glm::normalize(r.start.tangent), uni(g, 0.0, 1.0) < 0.3 ? 3.0 : 1.2);
        r.goal.positionMeters += randomUnit(g) * uni(g, 0.0, 500.0);
        r.goal.tangent = perturb(g, glm::normalize(r.goal.tangent), 0.5);
        r.protectedStartStraightMeters *= uni(g, 0.3, 2.0);
        r.protectedGoalStraightMeters *= uni(g, 0.5, 1.5);
    }
    r.start.speedMps = uni(g, 0.0, 1.0) < 0.2 ? 0.0 : uni(g, 0.0, r.designSpeedMps);
    r.goal.speedMps = std::min(r.designSpeedMps, uni(g, 0.0, 0.5) * r.designSpeedMps);
    r.samplesPerBezier = 96;
    const glm::dvec3 st = r.start.positionMeters + r.start.tangent * r.protectedStartStraightMeters;
    const glm::dvec3 gt = r.goal.positionMeters - r.goal.tangent * r.protectedGoalStraightMeters;
    return glm::length(gt - st) > 100.0 && !insideAny(r.obstacles, r.start.positionMeters, r.hullRadiusMeters) &&
           !insideAny(r.obstacles, r.goal.positionMeters, r.hullRadiusMeters) &&
           !insideAny(r.obstacles, st, r.hullRadiusMeters) && !insideAny(r.obstacles, gt, r.hullRadiusMeters);
}

lab::Result teacher(lab::Request r)
{
    r.weights = lab::timeOptimalWeights();
    r.maxPatternIterations = 480;
    return lab::ContinuousAssistedPathOptimizer::optimize(r);
}

double nowMs()
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::vector<lab::Request> loadScenes(int argc, char** argv, int first)
{
    std::vector<lab::Request> scenes;
    for (int i = first; i < argc; ++i)
        for (auto& r : game::navigation::planner::readRouteDraftRequests(argv[i]))
            if (r.designSpeedMps > 0.0)
                scenes.push_back(std::move(r));
    std::printf("scenes: %zu dumped requests\n", scenes.size());
    return scenes;
}

int dataset(int argc, char** argv)
{
    const char* outPath = argv[2];
    const int samples = std::atoi(argv[3]), threads = std::max(1, std::atoi(argv[4]));
    const double mazeShare = std::atof(argv[5]);
    const auto scenes = loadScenes(argc, argv, 6);
    std::FILE* out = std::fopen(outPath, "wb");
    if (!out)
        return 1;
    std::mutex mutex;
    std::atomic<int> next {0}, written {0}, valid {0};
    const double t0 = nowMs();
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&, t] {
            std::mt19937_64 g(0x5eed0000ull + static_cast<unsigned>(t) * 7919ull);
            for (int k = next++; k < samples; k = next++)
            {
                lab::Request r;
                int tries = 0;
                while (!sampleRequest(g, scenes, mazeShare, r) && ++tries < 50) {}
                if (tries >= 50)
                    continue;
                const auto res = teacher(r);
                const auto f = game::navigation::planner::routeDraftFrame(r);
                if (!f.valid)
                    continue;
                std::vector<float> row = game::navigation::planner::routeDraftFeatures(r, f);
                const auto y = game::navigation::planner::routeDraftTargets(res.shape, f);
                row.insert(row.end(), y.begin(), y.end());
                row.push_back(static_cast<float>(res.diagnostics.estimatedTravelTimeSeconds));
                row.push_back(res.valid ? 1.0f : 0.0f);
                std::lock_guard<std::mutex> lock(mutex);
                std::fwrite(row.data(), sizeof(float), row.size(), out);
                ++written;
                if (res.valid)
                    ++valid;
                if (written % 500 == 0)
                    std::printf("  %d rows (%d valid), %.0f s\n", written.load(), valid.load(), (nowMs() - t0) / 1000.0);
            }
        });
    for (auto& th : pool)
        th.join();
    std::fclose(out);
    std::printf("dataset: %d rows (%d valid), %d floats a row, %.0f s\n", written.load(), valid.load(),
                RouteDraftInputs + RouteDraftOutputs + 2, (nowMs() - t0) / 1000.0);
    return 0;
}

struct Stat
{
    int n = 0, valid = 0, draftFeasible = 0, fast = 0;
    double ms = 0.0, evals = 0.0, timeRatio = 0.0, detour = 0.0;
    int ratioN = 0;
};

int eval(int argc, char** argv)
{
    game::navigation::planner::RouteDraftNet net;
    if (!net.load(argv[2]))
    {
        std::printf("net %s did not load\n", argv[2]);
        return 1;
    }
    const int samples = std::atoi(argv[3]), threads = std::max(1, std::atoi(argv[4]));
    const double mazeShare = std::atof(argv[5]);
    const auto scenes = loadScenes(argc, argv, 6);
    const int refine = std::getenv("RD_REFINE") ? std::atoi(std::getenv("RD_REFINE")) : 40;
    std::mutex mutex;
    std::atomic<int> next {0};
    Stat game, teach, netS;
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back([&, t] {
            std::mt19937_64 g(0xe7a10000ull + static_cast<unsigned>(t) * 104729ull);   // not the dataset's seeds
            for (int k = next++; k < samples; k = next++)
            {
                lab::Request r;
                int tries = 0;
                while (!sampleRequest(g, scenes, mazeShare, r) && ++tries < 50) {}
                if (tries >= 50)
                    continue;
                const auto f = game::navigation::planner::routeDraftFrame(r);
                double t1 = nowMs();
                lab::Request rg = r;
                rg.maxPatternIterations = 160;
                const auto a = lab::ContinuousAssistedPathOptimizer::optimize(rg);
                const double msGame = nowMs() - t1;
                t1 = nowMs();
                const auto b = teacher(r);
                const double msTeach = nowMs() - t1;
                t1 = nowMs();
                lab::Request rn = r;
                rn.weights = lab::timeOptimalWeights();
                rn.maxPatternIterations = 160;
                lab::ShapeVariables draft;
                const bool haveDraft = net.draft(r, draft);
                if (haveDraft)
                {
                    rn.draftSeeds.push_back(draft);
                    rn.draftRefineIterations = static_cast<std::size_t>(refine);
                }
                const auto c = lab::ContinuousAssistedPathOptimizer::optimize(rn);
                const double msNet = nowMs() - t1;
                std::lock_guard<std::mutex> lock(mutex);
                const auto add = [&](Stat& s, const lab::Result& x, double ms) {
                    ++s.n;
                    s.ms += ms;
                    s.evals += static_cast<double>(x.diagnostics.objectiveEvaluations);
                    if (!x.valid)
                        return;
                    ++s.valid;
                    s.detour += x.diagnostics.pathLengthMeters / std::max(1.0, f.length);
                    if (b.valid)
                    {
                        s.timeRatio += x.diagnostics.estimatedTravelTimeSeconds /
                                       std::max(0.1, b.diagnostics.estimatedTravelTimeSeconds);
                        ++s.ratioN;
                    }
                };
                add(game, a, msGame);
                add(teach, b, msTeach);
                add(netS, c, msNet);
                if (c.usedDraftFastPath)
                    ++netS.fast;
            }
        });
    for (auto& th : pool)
        th.join();
    const auto print = [](const char* name, const Stat& s) {
        std::printf("%-8s n %d valid %5.1f %%  time/teacher %.3f  wall %7.1f ms  evals %6.1f  detour %.3f", name, s.n,
                    100.0 * s.valid / std::max(1, s.n), s.timeRatio / std::max(1, s.ratioN), s.ms / std::max(1, s.n),
                    s.evals / std::max(1, s.n), s.detour / std::max(1, s.valid));
        if (s.fast)
            std::printf("  draft fast path %5.1f %%", 100.0 * s.fast / std::max(1, s.n));
        std::printf("\n");
    };
    std::printf("maze share %.2f, net refine %d iterations\n", mazeShare, refine);
    print("game", game);
    print("teacher", teach);
    print("net", netS);
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 6)
    {
        std::printf("usage: EliteRouteDraftLab dataset <out.bin> <samples> <threads> <maze share> [dumps...]\n"
                    "       EliteRouteDraftLab eval <net.json> <samples> <threads> <maze share> [dumps...]\n");
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "dataset")
        return dataset(argc, argv);
    if (mode == "eval")
        return eval(argc, argv);
    return 2;
}
