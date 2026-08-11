#include "src/game/server/ServerRunner.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "src/game/network/IServerTransport.h"
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
    IServerTransport& transport,
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

void ServerRunner::receiveInboundMessages()
{
    EntityId playerId;
    game::network::ClientMessage clientMessage;
    while (m_transport.receiveClientMessage(playerId, clientMessage))
        m_server.receiveClientMessage(playerId, clientMessage);

    game::network::MapRequest mapRequest;
    while (m_transport.receiveMapRequest(mapRequest))
        m_server.enqueueMapRequest(mapRequest);

    game::network::PresentationDataRequest presentationRequest;
    while (m_transport.receivePresentationDataRequest(presentationRequest))
        m_server.enqueuePresentationDataRequest(presentationRequest);

    game::network::TimeSyncRequest timeSyncRequest;
    while (m_transport.receiveTimeSyncRequest(timeSyncRequest))
    {
        game::network::TimeSyncResponse response;
        response.sequence = timeSyncRequest.sequence;
        response.clientSendTimeSeconds =
            timeSyncRequest.clientSendTimeSeconds;
        response.serverReceiveTimeSeconds =
            m_server.serverTimeSeconds();

        m_transport.sendTimeSyncResponse(std::move(response));
    }
}

void ServerRunner::publishOutboundMessages()
{
    // The transport receives only a replicated value object; it never reaches
    // back into GameServer to discover or retain authoritative state.
    m_transport.publishSnapshot(m_server.snapshot());

    game::network::MapResponse mapResponse;
    while (m_server.popMapResponse(mapResponse))
        m_transport.sendMapResponse(std::move(mapResponse));

    game::network::PresentationDataResponse presentationResponse;
    while (m_server.popPresentationDataResponse(presentationResponse))
    {
        m_transport.sendPresentationDataResponse(
            std::move(presentationResponse)
        );
    }
}

void ServerRunner::runFixedStep()
{
    const float fixedStep =
        static_cast<float>(m_policy.fixedStepSeconds);

    // Preserve the established loopback ordering without allowing transport
    // code to call GameServer directly: older packets arrive first, commands
    // are consumed by this fixed step, then newly published state is exposed.
    m_transport.update(fixedStep);
    receiveInboundMessages();

    m_server.update(m_policy.fixedStepSeconds);

    publishOutboundMessages();
    m_transport.update(0.0f);
}
}
