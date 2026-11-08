#include "src/game/server/ServerRunner.h"
#include "src/game/server/ServerTickProfile.h"

#include <algorithm>
#include <optional>
#include <cmath>
#include <cstdlib>
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
    ServerTickPolicy policy
)
    : m_server(server)
    , m_policy(policy)
{
    validatePolicy(m_policy);
}

ServerRunner::ServerRunner(
    GameServer& server,
    IServerTransport& transport,
    game::network::ServerSessionId sessionId,
    ServerTickPolicy policy
)
    : ServerRunner(server, policy)
{
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

bool ServerRunner::seedTransportReplicationBaseline(
    game::network::ServerSessionId sessionId,
    const SimulationSnapshot& bootstrapSnapshot
)
{
    auto* binding = findBinding(sessionId);
    if (!binding || !binding->transport)
        return false;

    game::server::seedReplicationPublicationState(
        binding->replicationPublicationState,
        bootstrapSnapshot
    );
    binding->lastPublishedServerTick =
        bootstrapSnapshot.metadata.serverTick;
    return true;
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
    // the source id sets once per publication, shared by every destination
    std::optional<game::server::ReplicationSourceIds> sourceIds;
    // Staggered publication (load test 2026-11-08: a snapshot tick cost ~2x an ordinary one -
    // every session's selection / copy / encode on the build tick): the snapshot is built once
    // per interval; destination k publishes it (interval - ticks) later at the most, the
    // sessions spread over the interval by their id. A destination never waits longer than one
    // interval since its last publication (a forced rebuild cannot starve a late group).
    const std::uint64_t nowTick = m_server.serverTick();
    const std::uint64_t interval = std::max<std::uint64_t>(1, m_server.snapshotInterval());
    const std::uint64_t sinceBuild = nowTick >= sharedSnapshot.metadata.serverTick ? nowTick - sharedSnapshot.metadata.serverTick : 0;

    // Stage M7 consumes each destination's interest plan as real transport
    // cadence. The authoritative source snapshot remains full-presence; only
    // the connection-specific wire packet is sparse.
    for (auto& binding : m_transports)
    {
        if (!binding.transport)
            continue;

        if (binding.lastPublishedServerTick ==
            sharedSnapshot.metadata.serverTick)
        {
            continue;
        }
        {
            // DEV A/B: ELITE_DEV_SNAPSHOT_STAGGER=0 publishes every session on the build tick (the old cadence)
            static const bool stagger = [] {
                const char* v = std::getenv("ELITE_DEV_SNAPSHOT_STAGGER");
                return !(v && *v == '0');
            }();
            const std::uint64_t group = stagger ? (binding.sessionId.value + interval - 1) % interval : 0;   // the first session: group 0
            const bool overdue = binding.lastPublishTick != 0 && nowTick - binding.lastPublishTick >= interval;
            if (sinceBuild < group && !overdue)
                continue;
        }

        auto& profile = serverTickProfile();
        auto stageStart = std::chrono::steady_clock::now();
        const auto stage = [&stageStart](double& sink) {
            const auto now = std::chrono::steady_clock::now();
            sink += std::chrono::duration<double>(now - stageStart).count();
            stageStart = now;
        };
        binding.lastShipInterestPlan =
            m_server.shipReplicationInterestPlanForSession(
                binding.sessionId
            );
        stage(profile.sessionPlan);

        // Mutate cadence/lifecycle memory only after packet composition has
        // succeeded. A rejected/stale session must not silently advance its
        // publication clocks.
        auto nextPublicationState =
            binding.replicationPublicationState;

        const auto selection =
            game::server::selectReplicationPublications(
                binding.lastShipInterestPlan,
                sharedSnapshot,
                nextPublicationState,
                sourceIds ? *sourceIds : sourceIds.emplace(game::server::buildReplicationSourceIds(sharedSnapshot))
            );
        stage(profile.sessionSelect);

        SimulationSnapshot sessionSnapshot;
        int publishedSystemId = binding.lastPublishedSystemId;
        if (!m_server.copySparseSnapshotForSession(
                binding.sessionId,
                selection,
                sessionSnapshot,
                publishedSystemId))
        {
            continue;
        }
        binding.lastPublishedSystemId = publishedSystemId;
        stage(profile.sessionCopy);

        binding.transport->publishSnapshot(sessionSnapshot);
        stage(profile.sessionPublish);
        binding.replicationPublicationState =
            std::move(nextPublicationState);
        binding.lastPublishedServerTick =
            sharedSnapshot.metadata.serverTick;
        binding.lastPublishTick = nowTick;
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

    {
        ScopedTickTimer timer(serverTickProfile().inbound);
        for (auto& binding : m_transports)
            receiveInboundMessages(binding);
    }

    m_server.update(m_policy.fixedStepSeconds);
    ++serverTickProfile().ticks;

    publishOutboundMessages();

    for (auto& binding : m_transports)
    {
        if (binding.transport)
            binding.transport->update(0.0f);
    }
}
}
