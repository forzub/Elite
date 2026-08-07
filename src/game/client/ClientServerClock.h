#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <vector>

namespace game::client
{

/*
    Smooth estimator for the authoritative server simulation clock.

    A time-sync exchange gives one noisy observation:

        local midpoint <-> server receive time

    The estimator keeps a bounded window, rejects high-RTT samples and fits an
    affine relation between the two clocks. The visible estimate is never
    stepped after startup. Residual phase error is absorbed with a very small
    bounded correction rate, so packet arrival cannot create a visible
    slow/fast cycle.
*/
class ClientServerClock
{
public:
    struct Config
    {
        std::size_t sampleWindow = 96;
        std::size_t startupSamples = 6;
        std::size_t regressionSamples = 8;

        // PC steady clocks normally differ by tens of ppm. Keep a generous
        // safety envelope without allowing network noise to invent large
        // clock-rate changes.
        double maxRateErrorPpm = 500.0;

        // Residual phase error is paid back over this window, with an absolute
        // speed limit. 0.10% is visually negligible and 350x smaller than the
        // previous +/-35% correction.
        double phaseCorrectionWindowSeconds = 15.0;
        double maxPhaseCorrectionRate = 0.0010;

        // RTT samples above the best path by more than this are treated as
        // queue/jitter outliers for the affine fit.
        double rttSlackSeconds = 0.020;
        double rttSlackFraction = 0.35;
    };

    struct Sample
    {
        double localMidpointSeconds = 0.0;
        double serverTimeSeconds = 0.0;
        double rttSeconds = 0.0;
    };

    ClientServerClock() = default;

    explicit ClientServerClock(const Config& config)
        : m_config(config)
    {
    }

    void reset() noexcept
    {
        m_localTimeSeconds = 0.0;
        m_estimatedServerTimeSeconds = 0.0;
        m_modelAnchorLocalSeconds = 0.0;
        m_modelAnchorServerSeconds = 0.0;
        m_modelRate = 1.0;
        m_effectiveRate = 1.0;
        m_lastPhaseErrorSeconds = 0.0;
        m_minRttSeconds = 0.0;
        m_rttJitterSeconds = 0.0;
        m_ready = false;
        m_samples.clear();
    }

    void advance(double localDeltaSeconds) noexcept
    {
        if (!std::isfinite(localDeltaSeconds) || localDeltaSeconds <= 0.0)
            return;

        m_localTimeSeconds += localDeltaSeconds;

        if (!m_ready)
            return;

        const double target = modelServerTime(m_localTimeSeconds);
        const double phaseError =
            target - m_estimatedServerTimeSeconds;

        m_lastPhaseErrorSeconds = phaseError;

        const double correctionWindow =
            std::max(0.001, m_config.phaseCorrectionWindowSeconds);

        const double phaseRate =
            std::clamp(
                phaseError / correctionWindow,
                -std::abs(m_config.maxPhaseCorrectionRate),
                +std::abs(m_config.maxPhaseCorrectionRate)
            );

        m_effectiveRate =
            std::max(0.0, m_modelRate + phaseRate);

        m_estimatedServerTimeSeconds +=
            localDeltaSeconds * m_effectiveRate;
    }

    bool addSyncSample(
        double clientSendTimeSeconds,
        double clientReceiveTimeSeconds,
        double serverReceiveTimeSeconds
    ) noexcept
    {
        if (!std::isfinite(clientSendTimeSeconds) ||
            !std::isfinite(clientReceiveTimeSeconds) ||
            !std::isfinite(serverReceiveTimeSeconds) ||
            clientReceiveTimeSeconds < clientSendTimeSeconds)
        {
            return false;
        }

        Sample sample;
        sample.localMidpointSeconds =
            0.5 * (clientSendTimeSeconds + clientReceiveTimeSeconds);
        sample.serverTimeSeconds = serverReceiveTimeSeconds;
        sample.rttSeconds =
            clientReceiveTimeSeconds - clientSendTimeSeconds;

        m_samples.push_back(sample);

        const std::size_t window =
            std::max<std::size_t>(1, m_config.sampleWindow);

        while (m_samples.size() > window)
            m_samples.pop_front();

        rebuildModel();

        if (!m_ready &&
            m_samples.size() >=
                std::max<std::size_t>(1, m_config.startupSamples))
        {
            m_estimatedServerTimeSeconds =
                modelServerTime(m_localTimeSeconds);
            m_effectiveRate = m_modelRate;
            m_lastPhaseErrorSeconds = 0.0;
            m_ready = true;
        }

        return true;
    }

    bool synchronized() const noexcept
    {
        return m_ready;
    }

    double localTimeSeconds() const noexcept
    {
        return m_localTimeSeconds;
    }

    double estimatedServerTimeSeconds() const noexcept
    {
        return m_estimatedServerTimeSeconds;
    }

    double modelRate() const noexcept
    {
        return m_modelRate;
    }

    double effectiveRate() const noexcept
    {
        return m_effectiveRate;
    }

    double lastPhaseErrorSeconds() const noexcept
    {
        return m_lastPhaseErrorSeconds;
    }

    double minimumRttSeconds() const noexcept
    {
        return m_minRttSeconds;
    }

    double rttJitterSeconds() const noexcept
    {
        return m_rttJitterSeconds;
    }

    std::size_t sampleCount() const noexcept
    {
        return m_samples.size();
    }

private:
    double modelServerTime(double localTimeSeconds) const noexcept
    {
        return
            m_modelAnchorServerSeconds +
            (localTimeSeconds - m_modelAnchorLocalSeconds) * m_modelRate;
    }

    void rebuildModel() noexcept
    {
        if (m_samples.empty())
            return;

        double minRtt = std::numeric_limits<double>::infinity();
        for (const Sample& sample : m_samples)
            minRtt = std::min(minRtt, sample.rttSeconds);

        if (!std::isfinite(minRtt))
            return;

        m_minRttSeconds = minRtt;

        std::vector<Sample> accepted;
        accepted.reserve(m_samples.size());

        const double rttThreshold =
            minRtt +
            std::max(
                std::max(0.0, m_config.rttSlackSeconds),
                minRtt * std::max(0.0, m_config.rttSlackFraction)
            );

        for (const Sample& sample : m_samples)
        {
            if (sample.rttSeconds <= rttThreshold)
                accepted.push_back(sample);
        }

        const std::size_t required =
            std::min(
                m_samples.size(),
                std::max<std::size_t>(1, m_config.startupSamples)
            );

        if (accepted.size() < required)
        {
            accepted.assign(m_samples.begin(), m_samples.end());
            std::sort(
                accepted.begin(),
                accepted.end(),
                [](const Sample& lhs, const Sample& rhs)
                {
                    return lhs.rttSeconds < rhs.rttSeconds;
                }
            );
            accepted.resize(required);
        }

        // Robust RTT diagnostics used later by the render-delay policy.
        double meanRtt = 0.0;
        for (const Sample& sample : accepted)
            meanRtt += sample.rttSeconds;
        meanRtt /= static_cast<double>(accepted.size());

        double variance = 0.0;
        for (const Sample& sample : accepted)
        {
            const double d = sample.rttSeconds - meanRtt;
            variance += d * d;
        }
        variance /= static_cast<double>(accepted.size());
        m_rttJitterSeconds = std::sqrt(std::max(0.0, variance));

        if (accepted.size() <
            std::max<std::size_t>(2, m_config.regressionSamples))
        {
            std::vector<double> offsets;
            offsets.reserve(accepted.size());

            for (const Sample& sample : accepted)
            {
                offsets.push_back(
                    sample.serverTimeSeconds -
                    sample.localMidpointSeconds
                );
            }

            std::sort(offsets.begin(), offsets.end());
            const std::size_t mid = offsets.size() / 2;
            const double medianOffset =
                offsets.size() % 2 == 0
                    ? 0.5 * (offsets[mid - 1] + offsets[mid])
                    : offsets[mid];

            m_modelRate = 1.0;
            m_modelAnchorLocalSeconds = m_localTimeSeconds;
            m_modelAnchorServerSeconds =
                m_localTimeSeconds + medianOffset;
            return;
        }

        // Weighted affine regression. Lower RTT samples get more influence.
        double weightSum = 0.0;
        double localMean = 0.0;
        double serverMean = 0.0;

        for (const Sample& sample : accepted)
        {
            const double safeRtt = std::max(sample.rttSeconds, 1.0e-6);
            const double ratio = minRtt / safeRtt;
            const double weight = ratio * ratio;

            weightSum += weight;
            localMean += weight * sample.localMidpointSeconds;
            serverMean += weight * sample.serverTimeSeconds;
        }

        if (weightSum <= 0.0)
            return;

        localMean /= weightSum;
        serverMean /= weightSum;

        double covariance = 0.0;
        double varianceLocal = 0.0;

        for (const Sample& sample : accepted)
        {
            const double safeRtt = std::max(sample.rttSeconds, 1.0e-6);
            const double ratio = minRtt / safeRtt;
            const double weight = ratio * ratio;
            const double x = sample.localMidpointSeconds - localMean;
            const double y = sample.serverTimeSeconds - serverMean;

            covariance += weight * x * y;
            varianceLocal += weight * x * x;
        }

        double rate = 1.0;
        if (varianceLocal > 1.0e-12)
            rate = covariance / varianceLocal;

        const double maxRateError =
            std::abs(m_config.maxRateErrorPpm) * 1.0e-6;

        rate = std::clamp(
            rate,
            1.0 - maxRateError,
            1.0 + maxRateError
        );

        m_modelRate = rate;
        m_modelAnchorLocalSeconds = localMean;
        m_modelAnchorServerSeconds = serverMean;
    }

private:
    Config m_config;
    std::deque<Sample> m_samples;

    double m_localTimeSeconds = 0.0;
    double m_estimatedServerTimeSeconds = 0.0;

    double m_modelAnchorLocalSeconds = 0.0;
    double m_modelAnchorServerSeconds = 0.0;
    double m_modelRate = 1.0;
    double m_effectiveRate = 1.0;
    double m_lastPhaseErrorSeconds = 0.0;

    double m_minRttSeconds = 0.0;
    double m_rttJitterSeconds = 0.0;

    bool m_ready = false;
};

} // namespace game::client
