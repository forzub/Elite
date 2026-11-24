#pragma once

// THE ROUTE-DRAFT NET (author 2026-11-23: «сделать нейронку и загнать ее на обучение ... оптимум по времени»): from a
// CANONICAL view of a free leg (start / goal, the ship's physical state and authority, the obstacles around the chord)
// to the continuous optimizer's shape variables (ContinuousAssistedPathOptimizer: cubic -> straight -> cubic). Its
// answer is a DRAFT SEED: when feasible, the optimizer only refines it (draftRefineIterations) instead of the full
// 13-seed / 4-branch search; when not, the generic search runs as before - the net can make it faster, never unsafe.
// Teacher: the same optimizer with the TIME-OPTIMAL weights and a larger budget (tools: EliteRouteDraftLab).
//
// The canonical frame: origin = the start's curve point (after the protected straight), +X = the chord to the goal's
// curve point, +Y = the start tangent's part across the chord, Z = X x Y. Lengths / the chord length L.
// Inputs (RouteDraftInputs): log10(L / 1 km); the start / goal tangents (3 + 3); start / goal speed / design speed;
// the ship's authority as lengths / L: the turn radius at the design speed (lateral), design speed / max omega, the
// braking and the acceleration distance from the design speed, the hull radius, the protected straights; an occupancy
// grid of the obstacles round the chord (6 x 4 x 4 cells over x -0.25..1.25 L, y / z -0.75..0.75 L).
// Outputs (RouteDraftOutputs): the midpoint offset from the chord's middle / L (3), the cruise direction (3), the half
// straight / L, the four handles / L.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h"

namespace game::navigation::planner
{
constexpr int RouteDraftGridX = 6, RouteDraftGridYZ = 4;
constexpr int RouteDraftInputs = 1 + 6 + 2 + 7 + RouteDraftGridX * RouteDraftGridYZ * RouteDraftGridYZ;
constexpr int RouteDraftOutputs = 11;
// the meaning and order of the inputs (routeDraftFeatures): a model trained on another layout is refused (NET-1);
// bump with any change of the features, and write it into the model (tools/navigation_runtime/route_draft_train.py)
constexpr int RouteDraftFeatureVersion = 2;

struct RouteDraftFrame
{
    glm::dvec3 origin {0.0};
    glm::dvec3 middle {0.0};
    glm::dmat3 axes {1.0};   // columns: X, Y, Z in the scene frame
    double length = 1.0;
    bool valid = false;
};

inline glm::dvec3 routeDraftPerpendicular(const glm::dvec3& x)
{
    const glm::dvec3 a = std::abs(x.y) < 0.9 ? glm::dvec3(0.0, 1.0, 0.0) : glm::dvec3(1.0, 0.0, 0.0);
    return glm::normalize(a - x * glm::dot(a, x));
}

inline RouteDraftFrame routeDraftFrame(const assisted::lab::Request& r)
{
    RouteDraftFrame f;
    const glm::dvec3 st = glm::length(r.start.tangent) > 1.0e-9 ? glm::normalize(r.start.tangent) : glm::dvec3(0, 0, -1);
    const glm::dvec3 gt = glm::length(r.goal.tangent) > 1.0e-9 ? glm::normalize(r.goal.tangent) : st;
    const glm::dvec3 s = r.start.positionMeters + st * std::max(0.0, r.protectedStartStraightMeters);
    const glm::dvec3 g = r.goal.positionMeters - gt * std::max(0.0, r.protectedGoalStraightMeters);
    const glm::dvec3 chord = g - s;
    f.length = glm::length(chord);
    if (!(f.length > 1.0))
        return f;
    const glm::dvec3 x = chord / f.length;
    glm::dvec3 y = st - x * glm::dot(st, x);
    if (glm::length(y) < 1.0e-3)
        y = gt - x * glm::dot(gt, x);
    y = glm::length(y) < 1.0e-3 ? routeDraftPerpendicular(x) : glm::normalize(y);
    f.axes = glm::dmat3(x, y, glm::cross(x, y));
    f.origin = s;
    f.middle = 0.5 * (s + g);
    f.valid = true;
    return f;
}

inline glm::dvec3 routeDraftToFrame(const RouteDraftFrame& f, const glm::dvec3& v)
{
    return glm::transpose(f.axes) * v;
}

inline std::vector<float> routeDraftFeatures(const assisted::lab::Request& r, const RouteDraftFrame& f)
{
    std::vector<float> out;
    out.reserve(RouteDraftInputs);
    const double L = f.length;
    const double v = std::max(1.0, r.designSpeedMps);
    const auto push = [&](double x) { out.push_back(static_cast<float>(std::clamp(x, -50.0, 50.0))); };
    push(std::log10(L / 1000.0));
    for (const auto& t : {r.start.tangent, r.goal.tangent})
    {
        const glm::dvec3 c = routeDraftToFrame(f, glm::length(t) > 1.0e-9 ? glm::normalize(t) : glm::dvec3(1, 0, 0));
        push(c.x);
        push(c.y);
        push(c.z);
    }
    push(r.start.speedMps / v);
    push(r.goal.speedMps / v);
    // the ship's physical state: what it can do at this speed, as lengths against the leg
    push(v * v / std::max(0.1, r.maxLateralAccelerationMps2) / L);
    push(r.maxAngularVelocityRadPerSecond > 1.0e-6 ? v / r.maxAngularVelocityRadPerSecond / L : 0.0);
    push(v * v / (2.0 * std::max(0.1, r.brakingMps2)) / L);
    push(v * v / (2.0 * std::max(0.1, r.acceleratingMps2)) / L);
    push(r.hullRadiusMeters / L * 10.0);
    push(r.protectedStartStraightMeters / L);
    push(r.protectedGoalStraightMeters / L);
    // the obstacles round the chord: per cell 1 - exp(-occupied volume / cell volume)
    std::vector<double> grid(RouteDraftGridX * RouteDraftGridYZ * RouteDraftGridYZ, 0.0);
    const double x0 = -0.25 * L, cx = 1.5 * L / RouteDraftGridX;
    const double y0 = -0.75 * L, cy = 1.5 * L / RouteDraftGridYZ;
    const double cellVolume = cx * cy * cy;
    for (const auto& o : r.obstacles)
    {
        const glm::dvec3 p = routeDraftToFrame(f, o.centerMeters - f.origin);
        const int ix = static_cast<int>(std::floor((p.x - x0) / cx));
        const int iy = static_cast<int>(std::floor((p.y - y0) / cy));
        const int iz = static_cast<int>(std::floor((p.z - y0) / cy));
        if (ix < 0 || iy < 0 || iz < 0 || ix >= RouteDraftGridX || iy >= RouteDraftGridYZ || iz >= RouteDraftGridYZ)
            continue;
        const double rad = o.conservativeRadiusMeters() + o.requiredClearanceMeters + r.hullRadiusMeters;
        grid[static_cast<std::size_t>((ix * RouteDraftGridYZ + iy) * RouteDraftGridYZ + iz)] += 4.18879 * rad * rad * rad;
    }
    for (const double g : grid)
        push(1.0 - std::exp(-g / cellVolume));
    return out;
}

inline std::vector<float> routeDraftTargets(const assisted::lab::ShapeVariables& s, const RouteDraftFrame& f)
{
    const double L = f.length;
    const glm::dvec3 m = routeDraftToFrame(f, s.midpoint - f.middle) / L;
    const glm::dvec3 c = routeDraftToFrame(f, glm::length(s.cruiseDirection) > 1.0e-9 ? glm::normalize(s.cruiseDirection)
                                                                                       : glm::dvec3(1, 0, 0));
    return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z),
            static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z),
            static_cast<float>(s.halfStraightMeters / L),
            static_cast<float>(s.startBoundaryHandleMeters / L), static_cast<float>(s.startCruiseHandleMeters / L),
            static_cast<float>(s.goalCruiseHandleMeters / L), static_cast<float>(s.goalBoundaryHandleMeters / L)};
}

inline assisted::lab::ShapeVariables routeDraftShape(const std::vector<float>& o, const RouteDraftFrame& f)
{
    const double L = f.length;
    assisted::lab::ShapeVariables s;
    s.midpoint = f.middle + f.axes * (glm::dvec3(o[0], o[1], o[2]) * L);
    glm::dvec3 c = f.axes * glm::dvec3(o[3], o[4], o[5]);
    s.cruiseDirection = glm::length(c) > 1.0e-6 ? glm::normalize(c) : f.axes[0];
    const auto len = [&](float v) { return std::max(1.0, static_cast<double>(v) * L); };
    s.halfStraightMeters = std::max(0.0, static_cast<double>(o[6]) * L);
    s.startBoundaryHandleMeters = len(o[7]);
    s.startCruiseHandleMeters = len(o[8]);
    s.goalCruiseHandleMeters = len(o[9]);
    s.goalBoundaryHandleMeters = len(o[10]);
    return s;
}

// an MLP (tanh hidden layers), inputs / outputs standardized by the trainer's stats
class RouteDraftNet
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
            // THE MODEL CONTRACT (audit NET-1: only the stats' sizes were checked - ragged rows, a long bias, a wide
            // layer or a short std read past the heap in forward(); a model of another feature layout with the same
            // sizes loaded silently): the schema, the feature layout version, every layer's shape chained from 112 to
            // 11, finite values and positive stds - anything else = no model (the generic search, as without the net)
            const auto reject = [&](const char* why) {
                m_layers.clear();
                m_rejectReason = why;
                return false;
            };
            if (j.value("schema", std::string()) != "elite.route_draft_net.v1")
                return reject("schema is not elite.route_draft_net.v1");
            if (j.value("inputs", 0) != static_cast<int>(RouteDraftInputs) || j.value("outputs", 0) != static_cast<int>(RouteDraftOutputs))
                return reject("inputs / outputs differ from the code's feature layout");
            if (j.value("feature_version", RouteDraftFeatureVersion) != RouteDraftFeatureVersion)
                return reject("feature_version differs from the code's (the inputs mean something else)");
            std::size_t width = static_cast<std::size_t>(RouteDraftInputs);
            for (const auto& l : j.at("layers"))
            {
                Layer layer;
                const auto w = l.at("w").get<std::vector<std::vector<float>>>();
                layer.b = l.at("b").get<std::vector<float>>();
                if (w.empty() || w.size() != layer.b.size())
                    return reject("a layer's rows and bias differ");
                layer.in = w.front().size();
                if (layer.in != width)
                    return reject("a layer's input width does not chain");
                for (const auto& row : w)
                {
                    if (row.size() != layer.in)
                        return reject("a layer has ragged rows");
                    layer.w.insert(layer.w.end(), row.begin(), row.end());
                }
                width = layer.b.size();
                m_layers.push_back(std::move(layer));
            }
            if (m_layers.empty() || width != static_cast<std::size_t>(RouteDraftOutputs))
                return reject("the last layer is not the code's output width");
            m_inMean = j.at("in_mean").get<std::vector<float>>();
            m_inStd = j.at("in_std").get<std::vector<float>>();
            m_outMean = j.at("out_mean").get<std::vector<float>>();
            m_outStd = j.at("out_std").get<std::vector<float>>();
            const auto nIn = static_cast<std::size_t>(RouteDraftInputs), nOut = static_cast<std::size_t>(RouteDraftOutputs);
            if (m_inMean.size() != nIn || m_inStd.size() != nIn || m_outMean.size() != nOut || m_outStd.size() != nOut)
                return reject("the standardization stats have the wrong sizes");
            const auto finite = [](const std::vector<float>& v, bool positive) {
                for (const float x : v)
                    if (!std::isfinite(x) || (positive && !(x > 0.0f)))
                        return false;
                return true;
            };
            if (!finite(m_inMean, false) || !finite(m_outMean, false) || !finite(m_inStd, true) || !finite(m_outStd, true))
                return reject("a standardization stat is not finite / a std is not positive");
            for (const auto& l : m_layers)
                if (!finite(l.w, false) || !finite(l.b, false))
                    return reject("a weight is not finite");
        }
        catch (...)
        {
            m_layers.clear();
            m_rejectReason = "unreadable JSON";
            return false;
        }
        m_rejectReason.clear();
        return true;
    }
    bool loaded() const { return !m_layers.empty(); }
    const std::string& rejectReason() const { return m_rejectReason; }

    std::vector<float> forward(const std::vector<float>& features) const
    {
        if (features.size() != static_cast<std::size_t>(RouteDraftInputs) || m_layers.empty())
            return {};
        std::vector<float> a(features.size());
        for (std::size_t i = 0; i < a.size(); ++i)
            a[i] = (features[i] - m_inMean[i]) / m_inStd[i];
        for (std::size_t li = 0; li < m_layers.size(); ++li)
        {
            const auto& l = m_layers[li];
            std::vector<float> b(l.b.size());
            for (std::size_t o = 0; o < b.size(); ++o)
            {
                float s = l.b[o];
                const float* row = &l.w[o * l.in];
                for (std::size_t i = 0; i < l.in; ++i)
                    s += row[i] * a[i];
                b[o] = li + 1 < m_layers.size() ? std::tanh(s) : s;
            }
            a = std::move(b);
        }
        for (std::size_t i = 0; i < a.size(); ++i)
            a[i] = a[i] * m_outStd[i] + m_outMean[i];
        return a;
    }

    // the draft seed for a request (invalid frame -> none)
    bool draft(const assisted::lab::Request& r, assisted::lab::ShapeVariables& out) const
    {
        if (!loaded())
            return false;
        const RouteDraftFrame f = routeDraftFrame(r);
        if (!f.valid)
            return false;
        const std::vector<float> y = forward(routeDraftFeatures(r, f));
        if (y.size() != static_cast<std::size_t>(RouteDraftOutputs))
            return false;
        out = routeDraftShape(y, f);
        return true;
    }

private:
    struct Layer
    {
        std::vector<float> w, b;
        std::size_t in = 0;
    };
    std::string m_rejectReason;
    std::vector<Layer> m_layers;
    std::vector<float> m_inMean, m_inStd, m_outMean, m_outStd;
};
} // namespace game::navigation::planner
