#pragma once

// THE NET that learns the evasion teacher (MultiThreatEvasion.h): a small MLP (tanh) from a CANONICAL view of the scene
// to the acceleration. Trained offline (tools/navigation_runtime/evasion_train.py from EliteEvasionLab's dataset),
// weights in assets/data/navigation/evasion_net.json. Used with a GATE (proposeEvasion): the net's answer is scored by
// the teacher's own score and kept only when it is at least as safe as a quick check of a few sampled manoeuvres; else
// the full teacher runs. So the net can make it faster, never less safe.
//
// The canonical frame: +Z = the ship's nose, +X towards the most dangerous threat's sideways position (its part across
// the nose), Y = Z x X. Features: the caps (4) + the K most dangerous threats (8 each: position / 1000 m, velocity /
// 100 m/s, radius / 100 m, present) -> 4 + 8 K. Output: the acceleration / mainAccel (3).
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/game/navigation/avoidance/MultiThreatEvasion.h"

namespace game::navigation::avoidance
{
constexpr int EvasionNetThreats = 4;
constexpr int EvasionNetInputs = 4 + 8 * EvasionNetThreats;

struct EvasionCanonical
{
    glm::dmat3 toCanonical {1.0};   // rows: the canonical axes in the scene frame
    std::vector<float> features;
};

// the danger of a threat for the order of the features: its closest approach soonest and nearest
inline double evasionDanger(const EvasionThreatTrack& t, double horizon)
{
    const double vv = glm::dot(t.velocity, t.velocity);
    double tc = vv > 1.0e-9 ? -glm::dot(t.position, t.velocity) / vv : 0.0;
    tc = std::clamp(tc, 0.0, horizon);
    const double d = glm::length(t.position + t.velocity * tc) - t.radiusMeters;
    return d + 10.0 * tc;   // lower = more dangerous
}

inline EvasionCanonical evasionFeatures(const EvasionScene& sc)
{
    EvasionCanonical out;
    std::vector<const EvasionThreatTrack*> order;
    for (const auto& t : sc.threats)
        order.push_back(&t);
    std::sort(order.begin(), order.end(), [&](const auto* a, const auto* b) {
        return evasionDanger(*a, sc.horizonSeconds) < evasionDanger(*b, sc.horizonSeconds);
    });
    const glm::dvec3 z = glm::normalize(sc.forward);
    glm::dvec3 x(0.0);
    if (!order.empty())
    {
        const glm::dvec3 p = order.front()->position;
        x = p - z * glm::dot(p, z);
    }
    if (glm::length(x) < 1.0e-6)
        x = std::abs(z.y) < 0.9 ? glm::cross(glm::dvec3(0.0, 1.0, 0.0), z) : glm::cross(glm::dvec3(1.0, 0.0, 0.0), z);
    x = glm::normalize(x);
    const glm::dvec3 y = glm::cross(z, x);
    out.toCanonical = glm::transpose(glm::dmat3(x, y, z));
    auto& f = out.features;
    f.reserve(EvasionNetInputs);
    f.push_back(static_cast<float>(sc.caps.lateralAccelMps2 / 5.0));
    f.push_back(static_cast<float>(sc.caps.mainAccelMps2 / 20.0));
    f.push_back(static_cast<float>(sc.caps.turnRateRadPerS));
    f.push_back(static_cast<float>(sc.caps.radiusMeters / 100.0));
    for (int k = 0; k < EvasionNetThreats; ++k)
    {
        if (k < static_cast<int>(order.size()))
        {
            const glm::dvec3 p = out.toCanonical * order[static_cast<std::size_t>(k)]->position;
            const glm::dvec3 v = out.toCanonical * order[static_cast<std::size_t>(k)]->velocity;
            for (int i = 0; i < 3; ++i)
                f.push_back(static_cast<float>(p[i] / 1000.0));
            for (int i = 0; i < 3; ++i)
                f.push_back(static_cast<float>(v[i] / 100.0));
            f.push_back(static_cast<float>(order[static_cast<std::size_t>(k)]->radiusMeters / 100.0));
            f.push_back(1.0f);
        }
        else
            for (int i = 0; i < 8; ++i)
                f.push_back(0.0f);
    }
    return out;
}

class EvasionNet
{
public:
    bool load(const std::string& path)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        try
        {
            nlohmann::json j;
            in >> j;
            m_layers.clear();
            for (const auto& l : j.at("layers"))
            {
                Layer layer;
                layer.w = l.at("w").get<std::vector<std::vector<float>>>();
                layer.b = l.at("b").get<std::vector<float>>();
                m_layers.push_back(std::move(layer));
            }
            m_ready = !m_layers.empty() && !m_layers.front().w.empty() &&
                      static_cast<int>(m_layers.front().w.front().size()) == EvasionNetInputs;
        }
        catch (const std::exception&)
        {
            m_ready = false;
        }
        return m_ready;
    }
    bool ready() const { return m_ready; }

    // the net's acceleration in the scene frame
    glm::dvec3 infer(const EvasionScene& sc) const
    {
        const EvasionCanonical c = evasionFeatures(sc);
        std::vector<float> a = c.features, b;
        for (std::size_t li = 0; li < m_layers.size(); ++li)
        {
            const auto& L = m_layers[li];
            b.assign(L.b.begin(), L.b.end());
            for (std::size_t o = 0; o < L.w.size(); ++o)
            {
                float s = b[o];
                const auto& row = L.w[o];
                for (std::size_t i = 0; i < row.size() && i < a.size(); ++i)
                    s += row[i] * a[i];
                b[o] = li + 1 < m_layers.size() ? std::tanh(s) : s;
            }
            a.swap(b);
        }
        if (a.size() < 3)
            return glm::dvec3(0.0);
        const glm::dvec3 canon(a[0], a[1], a[2]);
        glm::dvec3 acc = glm::transpose(c.toCanonical) * canon * sc.caps.mainAccelMps2;
        const double m = glm::length(acc);
        if (m > sc.caps.mainAccelMps2)
            acc *= sc.caps.mainAccelMps2 / m;
        return acc;
    }

private:
    struct Layer
    {
        std::vector<std::vector<float>> w;   // [out][in]
        std::vector<float> b;
    };
    std::vector<Layer> m_layers;
    bool m_ready = false;
};

// the reachable version of a wanted acceleration (the main drive only along a direction it can turn to in time)
inline EvasionChoice scoreWanted(const EvasionScene& sc, const glm::dvec3& wanted)
{
    const double m = glm::length(wanted);
    if (m < 1.0e-6)
        return scoreEvasion(sc, glm::dvec3(0.0), 0.0);
    double delay = 0.0;
    const glm::dvec3 a = evasionAccelFor(sc, wanted / m, std::min(1.0, m / std::max(1.0e-6, sc.caps.mainAccelMps2)), &delay);
    return scoreEvasion(sc, a, delay);
}

// THE GATE: the net's answer when it is at least as safe as a quick check (16 directions at full thrust + nothing);
// else the full teacher. `usedNet` says which.
inline EvasionChoice proposeEvasion(const EvasionScene& sc, const EvasionNet* net, bool* usedNet = nullptr)
{
    if (usedNet)
        *usedNet = false;
    if (!net || !net->ready())
        return teachEvasion(sc);
    const EvasionChoice fromNet = scoreWanted(sc, net->infer(sc));
    EvasionChoice quick = scoreEvasion(sc, glm::dvec3(0.0), 0.0);
    const auto& dirs = evasionDirections();
    for (std::size_t i = 0; i < dirs.size(); i += dirs.size() / 16)
    {
        double delay = 0.0;
        const EvasionChoice c = scoreEvasion(sc, evasionAccelFor(sc, dirs[i], 1.0, &delay), delay);
        if (c.score > quick.score)
            quick = c;
    }
    // the net only when ITS answer is safe on its own (a net answer that merely beat the quick check while still unsafe
    // was worse than the teacher in 2 of 5000 scenes, EvasionLab eval 2026-11-23)
    if (fromNet.minMarginMeters >= 0.0 && fromNet.score >= quick.score)
    {
        if (usedNet)
            *usedNet = true;
        return fromNet;
    }
    return teachEvasion(sc);
}
} // namespace game::navigation::avoidance
