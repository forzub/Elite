#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/game/network/SessionMessage.h"
#include "src/game/server/ServerRunner.h"
#include "src/world/WorldParams.h"

namespace game::debug
{
class IServerDebugChannel;
}

namespace game::network
{
class TcpServerTransport;
class TcpServerListener;
}

namespace game::server
{
class ServerRuntime;

/*
    Dedicated TCP host around one authoritative ServerRuntime.

    Listener/socket details stay below TcpServerTransport. Admission is
    server-owned: a newly accepted connection never supplies an EntityId.
    This host owns connection lifetime and detaches session authority when a
    transport disconnects.
*/
class NetworkServerHost final
{
public:
    NetworkServerHost(
        const WorldParams& worldParams,
        game::debug::IServerDebugChannel& debugChannel
    );
    ~NetworkServerHost();

    NetworkServerHost(const NetworkServerHost&) = delete;
    NetworkServerHost& operator=(const NetworkServerHost&) = delete;

    bool listen(const std::string& bindAddress, std::uint16_t port);
    ServerAdvanceResult advance(double elapsedSeconds);

    bool listening() const noexcept;
    std::uint16_t localPort() const noexcept;
    std::size_t connectedSessionCount() const noexcept;
    std::uint64_t acceptedConnectionCount() const noexcept;
    double fixedStepSeconds() const;
    const std::string& lastError() const noexcept;

    void close();

private:
    struct Connection
    {
        std::uint64_t traceId = 0;
        std::unique_ptr<game::network::TcpServerTransport> transport;
        game::network::ServerSessionId sessionId {};
    };

    void acceptPendingConnections();
    void admitPendingConnections();
    void reapDisconnectedConnections();

    std::unique_ptr<game::network::TcpServerListener> m_listener;
    std::unique_ptr<ServerRuntime> m_runtime;
    std::vector<Connection> m_connections;
    std::uint64_t m_nextConnectionTraceId = 1;
    std::uint64_t m_acceptedConnectionCount = 0;
    std::string m_error;
};
}
