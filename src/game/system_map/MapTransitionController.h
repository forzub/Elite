#pragma once

#include <algorithm>
#include <functional>
#include <utility>

enum class MapTransitionCurve
{
    Linear,
    SmoothStep,
    SmootherStep
};

struct MapTransitionSpec
{
    double durationSeconds = 0.36;
    MapTransitionCurve curve = MapTransitionCurve::SmootherStep;
};

namespace MapTransitionPresets
{
    inline MapTransitionSpec selectionJump()
    {
        return {
            0.30,
            MapTransitionCurve::SmootherStep
        };
    }

    inline MapTransitionSpec modeChange()
    {
        return {
            0.72,
            MapTransitionCurve::SmootherStep
        };
    }
}

class MapTransitionController
{
public:
    bool begin(
        const MapTransitionSpec& spec,
        std::function<void()> applyNewState
    )
    {
        if (m_phase != Phase::Idle)
            return false;

        m_spec = spec;
        m_spec.durationSeconds =
            std::max(0.05, m_spec.durationSeconds);

        m_applyNewState =
            std::move(applyNewState);

        m_outgoingAlpha = 1.0f;
        m_phase = Phase::AwaitingCapture;

        return true;
    }

    bool needsOutgoingCapture() const
    {
        return m_phase == Phase::AwaitingCapture;
    }

    void outgoingCaptured(double nowSeconds)
    {
        if (m_phase != Phase::AwaitingCapture)
            return;

        auto action =
            std::move(m_applyNewState);

        m_applyNewState = {};
        m_startedAtSeconds = nowSeconds;
        m_outgoingAlpha = 1.0f;
        m_phase = Phase::Blending;

        // Состояние меняется только после сохранения старого кадра.
        if (action)
            action();
    }

    void update(double nowSeconds)
    {
        if (m_phase != Phase::Blending)
            return;

        const double elapsed =
            nowSeconds - m_startedAtSeconds;

        const double linearProgress =
            std::clamp(
                elapsed / m_spec.durationSeconds,
                0.0,
                1.0
            );

        const double progress =
            applyCurve(linearProgress);

        m_outgoingAlpha =
            static_cast<float>(1.0 - progress);

        if (linearProgress >= 1.0)
        {
            m_outgoingAlpha = 0.0f;
            m_phase = Phase::Idle;
        }
    }

    bool active() const
    {
        return m_phase != Phase::Idle;
    }

    bool blocksInput() const
    {
        return active();
    }

    float outgoingAlpha() const
    {
        return m_outgoingAlpha;
    }

private:
    enum class Phase
    {
        Idle,
        AwaitingCapture,
        Blending
    };

    double applyCurve(double value) const
    {
        switch (m_spec.curve)
        {
            case MapTransitionCurve::Linear:
                return value;

            case MapTransitionCurve::SmoothStep:
                return value * value * (3.0 - 2.0 * value);

            case MapTransitionCurve::SmootherStep:
            default:
                return
                    value * value * value *
                    (value * (value * 6.0 - 15.0) + 10.0);
        }
    }

private:
    Phase m_phase = Phase::Idle;
    MapTransitionSpec m_spec;

    std::function<void()> m_applyNewState;

    double m_startedAtSeconds = 0.0;
    float m_outgoingAlpha = 0.0f;
};