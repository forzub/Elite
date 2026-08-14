#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace game::client
{

/*
    Single client presentation playhead for authoritative world state.

    ClientServerClock estimates "server now". That estimate may temporarily
    diverge from snapshot history after a long client frame, a server hitch or
    deliberate fixed-step debt discard. Rendering directly at

        estimatedServerTime - interpolationDelay

    then exhausts the snapshot buffer and degenerates into latest-snapshot
    hold. The presentation clock keeps one continuous delayed playhead instead:

      * normal operation follows estimated server time - render delay;
      * the playhead is constrained by actually received snapshot history;
      * a large estimator/history disagreement is treated as a timeline
        discontinuity and rebased to a safe point behind the newest snapshot;
      * small residual error is slewed slowly instead of stepped.

    This is not a second authority clock. It is the one delayed render timeline
    derived from authoritative server time plus authoritative snapshot history.
*/
class ClientPresentationClock
{
public:
    struct Config
    {
        double interpolationDelaySeconds = 0.200;

        // Keep at least one normal publication interval between the render
        // playhead and the newest snapshot during ordinary operation.
        double minimumSnapshotLeadSeconds = 0.060;

        // After a large clock/history disagreement, rebuild a little more
        // history before continuing. Two 16.7 Hz publication intervals give
        // enough room for one delayed/lost packet without immediately starving.
        double recoverySnapshotLeadSeconds = 0.120;

        // Small target error is paid back gradually. The hard rebase below is
        // reserved for actual timeline discontinuities, not normal jitter.
        double phaseCorrectionWindowSeconds = 2.0;
        double maxRateCorrection = 0.005;

        // A quarter-second disagreement in either direction is far outside the
        // normal clock-sync jitter envelope and means the snapshot timeline and
        // estimated "now" no longer describe the same recent history.
        double hardRebaseThresholdSeconds = 0.250;
    };

    ClientPresentationClock() = default;

    explicit ClientPresentationClock(const Config& config)
        : m_config(config)
    {
    }

    void reset() noexcept
    {
        m_renderTimeSeconds = 0.0;
        m_ready = false;
        m_recoveryMode = false;
        m_hardRebaseCount = 0;
        m_starvationCount = 0;
        m_lastSnapshotLeadSeconds = 0.0;
        m_hasFirstSnapshot = false;
        m_firstSnapshotServerTimeSeconds = 0.0;
    }

    void update(
        double localDeltaSeconds,
        double estimatedServerTimeSeconds,
        bool hasSnapshot,
        double newestSnapshotServerTimeSeconds
    ) noexcept
    {
        const double dt =
            std::isfinite(localDeltaSeconds)
                ? std::max(0.0, localDeltaSeconds)
                : 0.0;

        if (!std::isfinite(estimatedServerTimeSeconds))
            return;

        const double nominalRenderTime =
            std::max(
                0.0,
                estimatedServerTimeSeconds -
                    std::max(0.0, m_config.interpolationDelaySeconds)
            );

        if (!hasSnapshot ||
            !std::isfinite(newestSnapshotServerTimeSeconds))
        {
            // A presentation playhead is meaningful only once authoritative
            // snapshot history exists. Before the first snapshot GameClient
            // already has a delayed server-clock fallback; marking this clock
            // ready here would freeze a provisional pre-admission epoch into
            // state. A remote client may connect to a server that has already
            // been running for seconds, leaving that provisional epoch far
            // behind the first received snapshot buffer.
            m_renderTimeSeconds = nominalRenderTime;
            m_ready = false;
            m_recoveryMode = false;
            m_lastSnapshotLeadSeconds = 0.0;
            return;
        }

        const double newest =
            std::max(0.0, newestSnapshotServerTimeSeconds);

        if (!m_hasFirstSnapshot)
        {
            m_hasFirstSnapshot = true;
            m_firstSnapshotServerTimeSeconds = newest;
        }
        const double minimumLead =
            std::max(0.0, m_config.minimumSnapshotLeadSeconds);
        const double recoveryLead =
            std::max(minimumLead, m_config.recoverySnapshotLeadSeconds);

        const double safeNominal =
            std::min(
                nominalRenderTime,
                std::max(0.0, newest - minimumLead)
            );

        const double estimatorLead = nominalRenderTime - newest;
        const bool largeHistoryDisagreement =
            std::abs(estimatorLead) >
                std::max(0.0, m_config.hardRebaseThresholdSeconds);

        if (!m_ready)
        {
            m_renderTimeSeconds = largeHistoryDisagreement
                ? std::max(0.0, newest - recoveryLead)
                : safeNominal;
            m_ready = true;
            m_recoveryMode = largeHistoryDisagreement;
        }
        else if (largeHistoryDisagreement)
        {
            const double recoveryTarget =
                std::max(0.0, newest - recoveryLead);

            if (!m_recoveryMode)
            {
                // The long wall-time step has already happened before this
                // update. Rebase once to the currently representable point;
                // do not integrate that same long dt a second time.
                m_renderTimeSeconds =
                    std::max(m_renderTimeSeconds, recoveryTarget);
                ++m_hardRebaseCount;
            }
            else
            {
                // Once in recovery, keep a continuous playhead. Newest
                // snapshot time advances in publication-sized steps, but the
                // render timeline advances every client frame.
                advanceToward(dt, recoveryTarget);
            }

            m_recoveryMode = true;
        }
        else
        {
            m_recoveryMode = false;
            advanceToward(dt, safeNominal);
        }

        // Snapshot history is a hard upper bound for interpolation. Reaching
        // it is starvation; hold rather than extrapolate here. Individual
        // presentation policies may add bounded extrapolation later.
        if (m_renderTimeSeconds > newest)
        {
            m_renderTimeSeconds = newest;

            const bool historyMature =
                m_hasFirstSnapshot &&
                newest - m_firstSnapshotServerTimeSeconds >= recoveryLead;

            if (historyMature)
                ++m_starvationCount;
        }

        m_lastSnapshotLeadSeconds =
            newest - m_renderTimeSeconds;
    }

    bool ready() const noexcept
    {
        return m_ready;
    }

    double renderTimeSeconds() const noexcept
    {
        return m_renderTimeSeconds;
    }

    bool recoveryMode() const noexcept
    {
        return m_recoveryMode;
    }

    std::uint64_t hardRebaseCount() const noexcept
    {
        return m_hardRebaseCount;
    }

    std::uint64_t starvationCount() const noexcept
    {
        return m_starvationCount;
    }

    double snapshotLeadSeconds() const noexcept
    {
        return m_lastSnapshotLeadSeconds;
    }

private:
    void advanceToward(double dt, double target) noexcept
    {
        if (dt <= 0.0)
            return;

        const double window =
            std::max(0.001, m_config.phaseCorrectionWindowSeconds);
        const double maxCorrection =
            std::abs(m_config.maxRateCorrection);
        const double error = target - m_renderTimeSeconds;
        const double correction =
            std::clamp(
                error / window,
                -maxCorrection,
                +maxCorrection
            );

        m_renderTimeSeconds += dt * (1.0 + correction);
    }

private:
    Config m_config;
    double m_renderTimeSeconds = 0.0;
    bool m_ready = false;
    bool m_recoveryMode = false;
    std::uint64_t m_hardRebaseCount = 0;
    std::uint64_t m_starvationCount = 0;
    double m_lastSnapshotLeadSeconds = 0.0;
    bool m_hasFirstSnapshot = false;
    double m_firstSnapshotServerTimeSeconds = 0.0;
};

} // namespace game::client
