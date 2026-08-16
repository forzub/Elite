#include "src/game/server/NetworkServerHost.h"
#include "src/core/RuntimeTrace.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

#include "src/game/debug/IServerDebugChannel.h"
#include "src/game/network/TcpTransport.h"
#include "src/game/server/ServerRuntime.h"

namespace game::server
{
NetworkServerHost::NetworkServerHost(
    const WorldParams& worldParams,
    game::debug::IServerDebugChannel& debugChannel
)
    : m_listener(std::make_unique<game::network::TcpServerListener>())
    , m_runtime(std::make_unique<ServerRuntime>(worldParams, debugChannel))
{
}

NetworkServerHost::~NetworkServerHost()
{
    close();
}

bool NetworkServerHost::listen(
    const std::string& bindAddress,
    std::uint16_t port)
{
    m_error.clear();
    if (!m_listener->listen(bindAddress, port))
    {
        m_error = m_listener->lastError();
        return false;
    }
    return true;
}

ServerAdvanceResult NetworkServerHost::advance(double elapsedSeconds)
{
    acceptPendingConnections();
    admitPendingConnections();
    const auto result = m_runtime->advance(elapsedSeconds);
    reapDisconnectedConnections();
    return result;
}

void NetworkServerHost::acceptPendingConnections()
{
    // Bound admission work per host iteration so a connection storm cannot
    // monopolize one simulation frame before the authoritative tick executes.
    constexpr std::size_t MaxAcceptsPerAdvance = 32;
    constexpr std::size_t MaxPendingAuthenticationConnections = 64;

    for (std::size_t i = 0; i < MaxAcceptsPerAdvance; ++i)
    {
        const std::size_t pendingAuthentication = static_cast<std::size_t>(
            std::count_if(
                m_connections.begin(),
                m_connections.end(),
                [](const Connection& connection)
                {
                    return connection.transport && !connection.sessionId;
                }
            )
        );
        if (pendingAuthentication >= MaxPendingAuthenticationConnections)
            break;

        auto transport = m_listener->acceptPending();
        if (!transport)
            break;

        // TCP accept is not gameplay admission. Keep the connection pending
        // until the client presents a SessionHello bearer token.
        Connection connection;
        connection.traceId = m_nextConnectionTraceId++;
        connection.transport = std::move(transport);
        connection.acceptedAt = std::chrono::steady_clock::now();
        if (core::runtimeTraceEnabled())
            std::cerr << "[M8E-CONNECT][server] accepted connection="
                      << connection.traceId
                      << " thread=" << std::this_thread::get_id() << "\n";
        m_connections.push_back(std::move(connection));
    }

    if (!m_listener->lastError().empty() && m_error.empty())
        m_error = m_listener->lastError();
}

void NetworkServerHost::admitPendingConnections()
{
    for (auto& connection : m_connections)
    {
        if (!connection.transport || connection.sessionId || connection.rejectionSent)
            continue;

        game::network::SessionHello hello;
        if (!connection.transport->receiveSessionHello(hello))
        {
            constexpr auto AuthenticationDeadline = std::chrono::seconds(10);
            if (std::chrono::steady_clock::now() - connection.acceptedAt >
                AuthenticationDeadline)
            {
                std::cerr << "[M8E-AUTH][server] handshake-timeout connection="
                          << connection.traceId << "\n";
                connection.transport->disconnect();
            }
            continue;
        }

        if (core::runtimeTraceEnabled())
            std::cerr << "[M8E-CONNECT][server] hello connection="
                      << connection.traceId
                      << " thread=" << std::this_thread::get_id() << "\n";

        using Clock = std::chrono::steady_clock;
        const auto admitBegin = Clock::now();
        const auto sessionId =
            m_runtime->attachPlayerSessionTransport(
                *connection.transport,
                hello
            );
        const double admitMs = std::chrono::duration<double, std::milli>(
            Clock::now() - admitBegin
        ).count();

        if (core::runtimeTraceEnabled())
            std::cerr << "[M8E-CONNECT][server] admission connection="
                      << connection.traceId
                      << " session=" << sessionId.value
                      << " ok=" << (sessionId ? "yes" : "no")
                      << " duration_ms=" << admitMs
                      << " thread=" << std::this_thread::get_id() << "\n";

        if (!sessionId)
        {
            // ServerRuntime has already queued one typed SessionReject. Keep
            // the socket alive long enough for the reliable stream to flush;
            // the client will close after consuming the rejection. A short
            // grace deadline prevents a rejected peer from lingering forever.
            connection.rejectionSent = true;
            connection.rejectionSentAt = std::chrono::steady_clock::now();
            connection.transport->update(0.0f);
            continue;
        }

        connection.sessionId = sessionId;
        ++m_acceptedConnectionCount;
    }
}

void NetworkServerHost::reapDisconnectedConnections()
{
    auto it = m_connections.begin();
    while (it != m_connections.end())
    {
        if (it->transport && !it->sessionId)
        {
            // Pending/rejected transports are not owned by ServerRunner yet,
            // so the host must pump their wire stream itself.
            it->transport->update(0.0f);

            constexpr auto RejectionFlushGrace = std::chrono::seconds(1);
            if (it->rejectionSent &&
                std::chrono::steady_clock::now() - it->rejectionSentAt >
                    RejectionFlushGrace)
            {
                it->transport->disconnect();
            }
        }

        if (it->transport && it->transport->connected())
        {
            ++it;
            continue;
        }

        if (it->sessionId)
            (void)m_runtime->detachPlayerSessionTransport(it->sessionId);

        it = m_connections.erase(it);
    }
}

bool NetworkServerHost::listening() const noexcept
{
    return m_listener && m_listener->listening();
}

std::uint16_t NetworkServerHost::localPort() const noexcept
{
    return m_listener ? m_listener->localPort() : 0u;
}

std::size_t NetworkServerHost::connectedSessionCount() const noexcept
{
    return m_runtime ? m_runtime->connectedPlayerSessionCount() : 0u;
}

std::uint64_t NetworkServerHost::acceptedConnectionCount() const noexcept
{
    return m_acceptedConnectionCount;
}

double NetworkServerHost::fixedStepSeconds() const
{
    return m_runtime->fixedStepSeconds();
}

const std::string& NetworkServerHost::lastError() const noexcept
{
    return m_error;
}

void NetworkServerHost::close()
{
    for (auto& connection : m_connections)
    {
        if (connection.transport)
            connection.transport->disconnect();
        if (connection.sessionId && m_runtime)
            (void)m_runtime->detachPlayerSessionTransport(connection.sessionId);
    }
    m_connections.clear();

    if (m_listener)
        m_listener->close();
}
}
