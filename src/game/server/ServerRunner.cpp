#include "src/game/server/ServerRunner.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "src/game/network/ITransport.h"
#include "src/game/server/GameServer.h"

namespace game::server
{
namespace
{
void validatePolicy(const ServerTickPolicy& policy)
{
    if (!std::isfinite(policy.fixedStepSeconds) ||
        policy.fixedStepSeconds <= 0.0)
    {
        throw std::invalid_argument(
            "ServerRunner fixedStepSeconds must be finite and positive"
        );
    }

    if (!std::isfinite(policy.maxFrameDeltaSeconds) ||
        policy.maxFrameDeltaSeconds < policy.fixedStepSeconds)
    {
        throw std::invalid_argument(
            "ServerRunner maxFrameDeltaSeconds must cover one fixed step"
        );
    }

    if (!std::isfinite(policy.maxAccumulatedDebtSeconds) ||
        policy.maxAccumulatedDebtSeconds < policy.fixedStepSeconds)
    {
        throw std::invalid_argument(
            "ServerRunner maxAccumulatedDebtSeconds must cover one fixed step"
        );
    }

    if (policy.maxCatchUpStepsPerAdvance == 0)
    {
        throw std::invalid_argument(
            "ServerRunner maxCatchUpStepsPerAdvance must be non-zero"
        );
    }
}
}

ServerRunner::ServerRunner(
    GameServer& server,
    ITransport& transport,
    ServerTickPolicy policy
)
    : m_server(server)
    , m_transport(transport)
    , m_policy(policy)
{
    validatePolicy(m_policy);
}

ServerAdvanceResult ServerRunner::advance(double elapsedSeconds)
{
    ServerAdvanceResult result;

    if (!std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0)
    {
        result.remainingDebtSeconds = m_accumulatorSeconds;
        result.totalDiscardedSeconds = m_totalDiscardedSeconds;
        return result;
    }

    result.acceptedElapsedSeconds =
        std::min(elapsedSeconds, m_policy.maxFrameDeltaSeconds);

    result.discardedSeconds =
        elapsedSeconds - result.acceptedElapsedSeconds;

    m_accumulatorSeconds += result.acceptedElapsedSeconds;

    if (m_accumulatorSeconds > m_policy.maxAccumulatedDebtSeconds)
    {
        result.discardedSeconds +=
            m_accumulatorSeconds - m_policy.maxAccumulatedDebtSeconds;

        m_accumulatorSeconds =
            m_policy.maxAccumulatedDebtSeconds;
    }

    m_totalDiscardedSeconds += result.discardedSeconds;

    const double stepTolerance =
        m_policy.fixedStepSeconds * 1.0e-9;

    while (
        m_accumulatorSeconds + stepTolerance >=
            m_policy.fixedStepSeconds &&
        result.stepsExecuted < m_policy.maxCatchUpStepsPerAdvance
    )
    {
        runFixedStep();

        m_accumulatorSeconds -= m_policy.fixedStepSeconds;
        result.stepsExecuted++;
    }

    if (m_accumulatorSeconds < stepTolerance)
        m_accumulatorSeconds = 0.0;

    result.simulatedSeconds =
        static_cast<double>(result.stepsExecuted) *
        m_policy.fixedStepSeconds;

    result.remainingDebtSeconds = m_accumulatorSeconds;
    result.totalDiscardedSeconds = m_totalDiscardedSeconds;
    result.catchUpLimited =
        m_accumulatorSeconds + stepTolerance >=
        m_policy.fixedStepSeconds;

    return result;
}

void ServerRunner::resetTiming()
{
    m_accumulatorSeconds = 0.0;
    m_totalDiscardedSeconds = 0.0;
}

void ServerRunner::runFixedStep()
{
    const float fixedStep =
        static_cast<float>(m_policy.fixedStepSeconds);

    // Preserve the established loopback order: deliver older packets,
    // advance the authoritative simulation, then expose the new snapshot.
    m_transport.update(fixedStep);
    m_server.update(m_policy.fixedStepSeconds);
    m_transport.update(0.0f);
}
}
