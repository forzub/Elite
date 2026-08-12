#include "src/game/server/ServerWorker.h"

#include <chrono>
#include <functional>
#include <utility>

#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/IServerTransport.h"
#include "src/game/server/ServerRuntime.h"

namespace game::server
{
ServerWorker::ServerWorker(
    const WorldParams& worldParams,
    IServerTransport& transport,
    game::debug::IServerDebugChannel& debugChannel
)
{
    // Construct ServerRuntime on the worker itself. From GameServer's first
    // constructor instruction onward, authoritative state belongs to exactly
    // one OS thread; there is no temporary main-thread ownership to forget
    // about when the execution model becomes more asynchronous later.
    m_thread = std::thread(
        &ServerWorker::threadMain,
        this,
        worldParams,
        std::ref(transport),
        std::ref(debugChannel)
    );

    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this]() {
        return m_started || static_cast<bool>(m_failure);
    });

    if (m_failure)
    {
        const auto failure = m_failure;
        lock.unlock();
        stopAndJoin();
        std::rethrow_exception(failure);
    }
}

ServerWorker::~ServerWorker()
{
    stopAndJoin();
}

ServerAdvanceResult ServerWorker::advance(double elapsedSeconds)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_failure)
        std::rethrow_exception(m_failure);

    // Keep the pipeline bounded to exactly one authoritative batch. If the
    // previous batch outlived the client/render frame, back-pressure is applied
    // here before accepting more elapsed time. We never queue an unbounded list
    // of frame deltas and never skip the fixed-step input stream to catch up.
    m_condition.wait(lock, [this]() {
        return !m_advancePending || static_cast<bool>(m_failure);
    });

    if (m_failure)
        std::rethrow_exception(m_failure);

    // Report the most recently completed authoritative batch. The batch being
    // submitted below intentionally remains in flight while the caller proceeds
    // with client prediction/presentation for this frame.
    const ServerAdvanceResult completedResult = m_lastAdvanceResult;

    m_pendingElapsedSeconds = elapsedSeconds;
    m_advancePending = true;
    m_condition.notify_all();

    return completedResult;
}

double ServerWorker::fixedStepSeconds() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_fixedStepSeconds;
}

void ServerWorker::threadMain(
    WorldParams worldParams,
    IServerTransport& transport,
    game::debug::IServerDebugChannel& debugChannel)
{
    try
    {
        // Runtime is a thread-local lifetime owner, not a member reachable by
        // LocalGameHost. It is destroyed on the same worker that constructed
        // and advanced it.
        ServerRuntime runtime(worldParams, transport, debugChannel);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_fixedStepSeconds = runtime.fixedStepSeconds();
            m_started = true;
        }
        m_condition.notify_all();

        while (true)
        {
            double elapsedSeconds = 0.0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this]() {
                    return m_stopRequested || m_advancePending;
                });

                if (m_stopRequested && !m_advancePending)
                    break;

                elapsedSeconds = m_pendingElapsedSeconds;
            }

            const auto executionStart = std::chrono::steady_clock::now();
            ServerAdvanceResult result = runtime.advance(elapsedSeconds);
            result.executionWallSeconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - executionStart
            ).count();

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastAdvanceResult = result;
                m_advancePending = false;
            }
            m_condition.notify_all();
        }
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_failure = std::current_exception();
            m_advancePending = false;
        }
        m_condition.notify_all();
    }
}

void ServerWorker::stopAndJoin() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = true;
    }
    m_condition.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}
}
