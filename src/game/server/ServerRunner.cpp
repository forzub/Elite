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
    game::network::ServerSessionId sessionId,
    ServerTickPolicy policy
)
    : m_server(server)
    , m_policy(policy)
{
    validatePolicy(m_policy);

    if (!attachTransport(transport, sessionId))
    {
        throw std::invalid_argument(
            "ServerRunner primary transport/session binding is invalid"
        );
    }
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

bool ServerRunner::attachTransport(
    IServerTransport& transport,
    game::network::ServerSessionId sessionId
)
{
    if (!sessionId)
        return false;

    const auto duplicate = std::find_if(
        m_transports.begin(),
        m_transports.end(),
        [&](const ServerTransportBinding& binding)
        {
            return
                binding.sessionId == sessionId ||
                binding.transport == &transport;
        }
    );

    if (duplicate != m_transports.end())
        return false;

    ServerTransportBinding binding;
    binding.transport = &transport;
    binding.sessionId = sessionId;
    // ServerRuntime immediately publishes bootstrap state when a transport is
    // admitted. Start at the current publication tick so the normal runner
    // does not enqueue the same snapshot again on the next fixed step.
    binding.lastPublishedServerTick =
        m_server.snapshot().metadata.serverTick;

    m_transports.push_back(binding);
    return true;
}

bool ServerRunner::detachTransport(
    game::network::ServerSessionId sessionId
)
{
    const auto it = std::find_if(
        m_transports.begin(),
        m_transports.end(),
        [&](const ServerTransportBinding& binding)
        {
            return binding.sessionId == sessionId;
        }
    );

    if (it == m_transports.end())
        return false;

    m_transports.erase(it);
    return true;
}

std::size_t ServerRunner::transportCount() const noexcept
{
    return m_transports.size();
}

ServerTransportBinding* ServerRunner::findBinding(
    game::network::ServerSessionId sessionId
) noexcept
{
    const auto it = std::find_if(
        m_transports.begin(),
        m_transports.end(),
        [&](const ServerTransportBinding& binding)
        {
            return binding.sessionId == sessionId;
        }
    );

    return it == m_transports.end() ? nullptr : &*it;
}

void ServerRunner::receiveInboundMessages(
    ServerTransportBinding& binding
)
{
    if (!binding.transport)
        return;

    auto& transport = *binding.transport;

    game::network::ClientMessage clientMessage;
    while (transport.receiveClientMessage(clientMessage))
    {
        // Each endpoint is bound once to a server-owned session. Packets carry
        // intent only; they never select another controlled entity.
        m_server.receiveClientMessage(
            binding.sessionId,
            clientMessage
        );
    }

    game::network::MapRequest mapRequest;
    while (transport.receiveMapRequest(mapRequest))
    {
        m_server.enqueueMapRequest(
            binding.sessionId,
            mapRequest
        );
    }

    game::network::TimeSyncRequest timeSyncRequest;
    while (transport.receiveTimeSyncRequest(timeSyncRequest))
    {
        game::network::TimeSyncResponse response;
        response.sequence = timeSyncRequest.sequence;
        response.clientSendTimeSeconds =
            timeSyncRequest.clientSendTimeSeconds;
        response.serverReceiveTimeSeconds =
            m_server.serverTimeSeconds();

        // Time sync is connection-local and can return directly through the
        // same endpoint; it never enters a shared response queue.
        transport.sendTimeSyncResponse(std::move(response));
    }
}

void ServerRunner::publishOutboundMessages()
{
    const auto& sharedSnapshot = m_server.snapshot();

    // Ordinary replication is currently full-world/full-presence, but the
    // session envelope is already distinct per connection. This is the seam
    // where per-client interest/sparse replication will be inserted later.
    for (auto& binding : m_transports)
    {
        if (!binding.transport)
            continue;

        if (binding.lastPublishedServerTick ==
            sharedSnapshot.metadata.serverTick)
        {
            continue;
        }

        SimulationSnapshot sessionSnapshot;
        if (!m_server.copySnapshotForSession(
                binding.sessionId,
                sessionSnapshot))
        {
            continue;
        }

        binding.transport->publishSnapshot(sessionSnapshot);
        binding.lastPublishedServerTick =
            sharedSnapshot.metadata.serverTick;
    }

    game::network::ServerSessionId responseSessionId;
    game::network::MapResponse mapResponse;
    while (m_server.popMapResponse(responseSessionId, mapResponse))
    {
        auto* binding = findBinding(responseSessionId);
        if (!binding || !binding->transport)
            continue;

        binding->transport->sendMapResponse(std::move(mapResponse));
    }
}

void ServerRunner::runFixedStep()
{
    const float fixedStep =
        static_cast<float>(m_policy.fixedStepSeconds);

    // Every attached transport advances/delivers its own latency or socket
    // queues before the single authoritative simulation step. No connection
    // owns a private GameServer tick.
    for (auto& binding : m_transports)
    {
        if (binding.transport)
            binding.transport->update(fixedStep);
    }

    for (auto& binding : m_transports)
        receiveInboundMessages(binding);

    m_server.update(m_policy.fixedStepSeconds);

    publishOutboundMessages();

    for (auto& binding : m_transports)
    {
        if (binding.transport)
            binding.transport->update(0.0f);
    }
}
}
