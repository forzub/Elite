#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "src/game/network/ITransport.h"
#include "src/game/network/IServerTransport.h"

namespace game::network::wire
{
class TcpWireStream;
class TcpWireListener;
}

namespace game::network
{

/*
    Game protocol adapters over TcpWireStream.

    These classes translate typed ITransport/IServerTransport messages to the
    portable wire codec. Socket ownership/framing remains in TcpWireStream;
    gameplay/server layers remain unaware of Asio or OS socket APIs.
*/
class TcpClientTransport final : public ITransport
{
public:
    TcpClientTransport();
    ~TcpClientTransport();

    TcpClientTransport(const TcpClientTransport&) = delete;
    TcpClientTransport& operator=(const TcpClientTransport&) = delete;

    bool connect(const std::string& host, std::uint16_t port);
    void service();
    void disconnect();

    bool connected() const noexcept;
    const std::string& lastError() const noexcept;

    void sendSessionHello(const SessionHello& hello) override;
    bool receiveSessionReject(SessionReject& outReject) override;
    bool receiveSessionWelcome(SessionWelcome& outWelcome) override;
    bool receiveSnapshot(SimulationSnapshot& outSnapshot) override;
    void sendClientMessage(const ClientMessage& msg) override;
    void sendMapRequest(const MapRequest& request) override;
    bool receiveMapResponse(MapResponse& outResponse) override;
    void sendTimeSyncRequest(const TimeSyncRequest& request) override;
    bool receiveTimeSyncResponse(TimeSyncResponse& outResponse) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class TcpServerTransport final : public IServerTransport
{
public:
    ~TcpServerTransport();

    TcpServerTransport(const TcpServerTransport&) = delete;
    TcpServerTransport& operator=(const TcpServerTransport&) = delete;

    void update(float dt) override;

    bool receiveSessionHello(SessionHello& outHello) override;
    bool receiveClientMessage(ClientMessage& outMessage) override;
    bool receiveMapRequest(MapRequest& outRequest) override;
    bool receiveTimeSyncRequest(TimeSyncRequest& outRequest) override;

    void publishSessionRejectImmediately(
        const SessionReject& reject) override;
    void publishSessionWelcomeImmediately(
        const SessionWelcome& welcome) override;
    void publishSnapshot(const SimulationSnapshot& snapshot) override;
    void publishSnapshotImmediately(
        const SimulationSnapshot& snapshot) override;
    void sendMapResponse(MapResponse response) override;
    void sendTimeSyncResponse(TimeSyncResponse response) override;

    bool connected() const noexcept;
    void disconnect();
    const std::string& lastError() const noexcept;

private:
    explicit TcpServerTransport(
        std::unique_ptr<wire::TcpWireStream> stream
    );

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    friend class TcpServerListener;
};

class TcpServerListener final
{
public:
    TcpServerListener();
    ~TcpServerListener();

    TcpServerListener(const TcpServerListener&) = delete;
    TcpServerListener& operator=(const TcpServerListener&) = delete;

    bool listen(
        const std::string& bindAddress,
        std::uint16_t port
    );

    std::unique_ptr<TcpServerTransport> acceptPending();

    bool listening() const noexcept;
    std::uint16_t localPort() const noexcept;
    void close();
    const std::string& lastError() const noexcept;

private:
    std::unique_ptr<wire::TcpWireListener> m_listener;
};

} // namespace game::network
