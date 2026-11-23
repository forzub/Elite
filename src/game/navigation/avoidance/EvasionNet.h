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
#include <stdexcept>
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
                const auto w = l.at("w").get<std::vector<std::vector<float>>>();
                layer.b = l.at("b").get<std::vector<float>>();
                layer.in = w.empty() ? 0 : w.front().size();
                for (const auto& row : w)
                {
                    if (row.size() != layer.in)
                        throw std::runtime_error("ragged layer");
                    layer.w.insert(layer.w.end(), row.begin(), row.end());
                }
                if (w.size() != layer.b.size())
                    throw std::runtime_error("bias size");
                m_layers.push_back(std::move(layer));
            }
            m_ready = !m_layers.empty() && static_cast<int>(m_layers.front().in) == EvasionNetInputs;
            for (std::size_t i = 1; i < m_layers.size(); ++i)
                m_ready = m_ready && m_layers[i].in == m_layers[i - 1].b.size();
        }
        catch (const std::exception&)
        {
            m_ready = false;
        }
        return m_ready;
    }
    bool ready() const { return m_ready; }
    std::size_t outputs() const { return m_layers.empty() ? 0 : m_layers.back().b.size(); }

    // the raw outputs for the canonical features
    std::vector<float> forward(const std::vector<float>& features) const
    {
        std::vector<float> a = features, b;
        for (std::size_t li = 0; li < m_layers.size(); ++li)
        {
            const auto& L = m_layers[li];
            b.assign(L.b.begin(), L.b.end());
            const std::size_t n = std::min(L.in, a.size());
            const float* row = L.w.data();
            for (std::size_t o = 0; o < b.size(); ++o, row += L.in)
            {
                float s = b[o];
                for (std::size_t i = 0; i < n; ++i)
                    s += row[i] * a[i];
                b[o] = li + 1 < m_layers.size() ? std::tanh(s) : s;
            }
            a.swap(b);
        }
        return a;
    }

    // the net's acceleration in the scene frame
    glm::dvec3 infer(const EvasionScene& sc) const
    {
        const EvasionCanonical c = evasionFeatures(sc);
        const std::vector<float> a = forward(c.features);
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
        std::vector<float> w;   // [out][in], row-major
        std::vector<float> b;
        std::size_t in = 0;
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

// THE CLASSIFIER variant (author 2026-11-23: «может пересмотреть количество узлов и связей?»): the net scores the
// teacher's own manoeuvre set, expressed in the CANONICAL frame (96 directions x 3 thrusts + nothing = 289 classes);
// the K best are scored by the teacher's score and the best kept. A regression net averages the equally good dodges
// (left / right) into the bad middle; choosing among the manoeuvres cannot.
constexpr int EvasionClassCount = 96 * 3 + 1;
constexpr int EvasionClassNothing = 96 * 3;

// the class of a teacher's choice: its direction taken to the canonical frame, the nearest canonical direction
inline int evasionClassOf(const EvasionCanonical& c, int dirIndex, int level)
{
    if (dirIndex < 0)
        return EvasionClassNothing;
    const auto& dirs = evasionDirections();
    const glm::dvec3 d = c.toCanonical * dirs[static_cast<std::size_t>(dirIndex)];
    int best = 0;
    double bestDot = -2.0;
    for (std::size_t i = 0; i < dirs.size(); ++i)
    {
        const double k = glm::dot(dirs[i], d);
        if (k > bestDot)
        {
            bestDot = k;
            best = static_cast<int>(i);
        }
    }
    return best * 3 + level;
}

inline EvasionChoice scoreEvasionClass(const EvasionScene& sc, const EvasionCanonical& c, int cls)
{
    if (cls == EvasionClassNothing)
        return scoreEvasion(sc, glm::dvec3(0.0), 0.0);
    const glm::dvec3 dir = glm::transpose(c.toCanonical) * evasionDirections()[static_cast<std::size_t>(cls / 3)];
    double delay = 0.0;
    const glm::dvec3 a = evasionAccelFor(sc, dir, EvasionThrustLevels[cls % 3], &delay);
    return scoreEvasion(sc, a, delay);
}

// the classifier's gate: the K best classes (+ nothing) by the teacher's score; when none of them is safe, the full
// teacher (it may still find a way the net did not rank)
inline EvasionChoice proposeEvasionTopK(const EvasionScene& sc, const EvasionNet& net, int k, bool* usedNet = nullptr)
{
    if (usedNet)
        *usedNet = false;
    const EvasionCanonical c = evasionFeatures(sc);
    const std::vector<float> logits = net.forward(c.features);
    if (static_cast<int>(logits.size()) != EvasionClassCount)
        return teachEvasion(sc);
    std::vector<int> order(EvasionClassCount);
    for (int i = 0; i < EvasionClassCount; ++i)
        order[static_cast<std::size_t>(i)] = i;
    k = std::clamp(k, 1, EvasionClassCount);
    std::partial_sort(order.begin(), order.begin() + k, order.end(),
                      [&](int a, int b) { return logits[static_cast<std::size_t>(a)] > logits[static_cast<std::size_t>(b)]; });
    EvasionChoice best = scoreEvasion(sc, glm::dvec3(0.0), 0.0);
    for (int i = 0; i < k; ++i)
    {
        const EvasionChoice ch = scoreEvasionClass(sc, c, order[static_cast<std::size_t>(i)]);
        if (ch.score > best.score)
            best = ch;
    }
    if (best.minMarginMeters >= 0.0)
    {
        if (usedNet)
            *usedNet = true;
        return best;
    }
    return teachEvasion(sc);
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
