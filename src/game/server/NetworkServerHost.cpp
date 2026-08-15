#include "src/game/server/NetworkServerHost.h"

#include <algorithm>
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

    for (std::size_t i = 0; i < MaxAcceptsPerAdvance; ++i)
    {
        auto transport = m_listener->acceptPending();
        if (!transport)
            break;

        // TCP accept is not gameplay admission. Keep the connection pending
        // until the client presents a SessionHello bearer token.
        Connection connection;
        connection.transport = std::move(transport);
        m_connections.push_back(std::move(connection));
    }

    if (!m_listener->lastError().empty() && m_error.empty())
        m_error = m_listener->lastError();
}

void NetworkServerHost::admitPendingConnections()
{
    for (auto& connection : m_connections)
    {
        if (!connection.transport || connection.sessionId)
            continue;

        game::network::SessionHello hello;
        if (!connection.transport->receiveSessionHello(hello))
            continue;

        const auto sessionId =
            m_runtime->attachPlayerSessionTransport(
                *connection.transport,
                hello
            );

        if (!sessionId)
        {
            // Unknown/full enrollment capacity or duplicate active login
            // are all authoritative admission failures. The client never gets
            // to fall back to selecting another PlayerId or EntityId.
            connection.transport->disconnect();
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
