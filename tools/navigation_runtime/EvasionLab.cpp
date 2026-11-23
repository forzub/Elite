// EliteEvasionLab - the evasion teacher's dataset and the net's check (src/game/navigation/avoidance/).
//   EliteEvasionLab dataset <scenes> <out.csv> [seed]   features + the teacher's acceleration (canonical, / mainAccel)
//   EliteEvasionLab eval <weights.json> <scenes> [seed]  the gate (net + quick check) against the full teacher
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

#include "src/game/navigation/avoidance/EvasionNet.h"

using namespace game::navigation::avoidance;

namespace
{
glm::dvec3 randomUnit(std::mt19937& rng)
{
    std::normal_distribution<double> n(0.0, 1.0);
    glm::dvec3 v(n(rng), n(rng), n(rng));
    const double m = glm::length(v);
    return m > 1.0e-9 ? v / m : glm::dvec3(0.0, 0.0, 1.0);
}

// a scene like a crowded hub: 1..4 tracks, most on a collision course (a CPA within 0..120 m in 3..25 s), some passing
EvasionScene randomScene(std::mt19937& rng)
{
    std::uniform_real_distribution<double> u(0.0, 1.0);
    EvasionScene sc;
    sc.forward = randomUnit(rng);
    sc.caps.lateralAccelMps2 = 0.5 + 2.5 * u(rng);
    sc.caps.mainAccelMps2 = 2.0 + 18.0 * u(rng);
    sc.caps.turnRateRadPerS = 0.1 + 0.7 * u(rng);
    sc.caps.radiusMeters = 10.0 + 70.0 * u(rng);
    const int n = 1 + static_cast<int>(u(rng) * 4.0);
    for (int i = 0; i < n; ++i)
    {
        EvasionThreatTrack t;
        t.radiusMeters = 10.0 + 110.0 * u(rng);
        const double speed = 5.0 + 295.0 * u(rng) * u(rng);
        const glm::dvec3 vdir = randomUnit(rng);
        t.velocity = vdir * speed;
        const bool collision = u(rng) < 0.75;
        const double tc = 3.0 + 22.0 * u(rng);
        glm::dvec3 miss = randomUnit(rng);
        miss -= vdir * glm::dot(miss, vdir);
        miss = glm::length(miss) > 1.0e-6 ? glm::normalize(miss) : glm::dvec3(1.0, 0.0, 0.0);
        const double missM = collision ? 120.0 * u(rng) : 150.0 + 800.0 * u(rng);
        // at tc the threat is at `miss * missM` from the ship (the ship coasting at the origin of its frame)
        t.position = miss * missM - t.velocity * tc;
        sc.threats.push_back(t);
    }
    return sc;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "usage: EliteEvasionLab dataset <scenes> <out.csv> [seed] | eval <weights.json> <scenes> [seed]\n";
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "dataset")
    {
        const int scenes = std::atoi(argv[2]);
        std::mt19937 rng(argc > 4 ? static_cast<unsigned>(std::atoi(argv[4])) : 1u);
        std::ofstream out(argv[3]);
        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < scenes; ++s)
        {
            const EvasionScene sc = randomScene(rng);
            const EvasionCanonical c = evasionFeatures(sc);
            const EvasionChoice best = teachEvasion(sc);
            const glm::dvec3 target = c.toCanonical * best.accel / sc.caps.mainAccelMps2;
            for (const float f : c.features)
                out << f << ",";
            out << target.x << "," << target.y << "," << target.z << "," << best.minMarginMeters << "\n";
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::cerr << "[EvasionLab] " << scenes << " scenes, teacher " << ms / scenes << " ms per scene\n";
        return 0;
    }
    if (mode == "eval")
    {
        EvasionNet net;
        if (!net.load(argv[2]))
        {
            std::cerr << "[EvasionLab] cannot load " << argv[2] << "\n";
            return 1;
        }
        const int scenes = std::atoi(argv[3]);
        std::mt19937 rng(argc > 4 ? static_cast<unsigned>(std::atoi(argv[4])) : 99u);
        int usedNet = 0, teacherSafe = 0, gateSafe = 0, netOnlySafe = 0, worse = 0;
        double teacherMs = 0.0, gateMs = 0.0, netMs = 0.0, marginLoss = 0.0;
        for (int s = 0; s < scenes; ++s)
        {
            const EvasionScene sc = randomScene(rng);
            auto t0 = std::chrono::steady_clock::now();
            const EvasionChoice teacher = teachEvasion(sc);
            auto t1 = std::chrono::steady_clock::now();
            bool fromNet = false;
            const EvasionChoice gate = proposeEvasion(sc, &net, &fromNet);
            auto t2 = std::chrono::steady_clock::now();
            const EvasionChoice raw = scoreWanted(sc, net.infer(sc));
            auto t3 = std::chrono::steady_clock::now();
            teacherMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
            gateMs += std::chrono::duration<double, std::milli>(t2 - t1).count();
            netMs += std::chrono::duration<double, std::milli>(t3 - t2).count();
            usedNet += fromNet ? 1 : 0;
            teacherSafe += teacher.minMarginMeters >= 0.0 ? 1 : 0;
            gateSafe += gate.minMarginMeters >= 0.0 ? 1 : 0;
            netOnlySafe += raw.minMarginMeters >= 0.0 ? 1 : 0;
            if (teacher.minMarginMeters >= 0.0 && gate.minMarginMeters < 0.0)
                ++worse;
            marginLoss += std::min(teacher.minMarginMeters, 200.0) - std::min(gate.minMarginMeters, 200.0);
        }
        std::printf("[EvasionLab] %d scenes: net used by the gate %.1f %%\n", scenes, 100.0 * usedNet / scenes);
        std::printf("  safe (margin >= 0): teacher %.1f %%, gate %.1f %%, the net alone %.1f %%; gate unsafe where the teacher is safe: %d\n",
                    100.0 * teacherSafe / scenes, 100.0 * gateSafe / scenes, 100.0 * netOnlySafe / scenes, worse);
        std::printf("  mean margin lost vs the teacher %.1f m; time per scene: teacher %.3f ms, gate %.3f ms, net + its score %.3f ms\n",
                    marginLoss / scenes, teacherMs / scenes, gateMs / scenes, netMs / scenes);
        return 0;
    }
    std::cerr << "unknown mode " << mode << "\n";
    return 2;
}
