#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/game/network/WireProtocol.h"

namespace game::network::wire
{

/*
    Schema-blind reliable ordered byte stream.

    This is the only layer that knows TCP exists. It transports framed opaque
    payload bytes, enforces per-direction frame sequencing and bounds the
    pending write queue. It deliberately knows nothing about snapshots, ships,
    maps, sessions or compressor internals.
*/
inline constexpr std::size_t MaxTcpQueuedWireBytes =
    64u * 1024u * 1024u;

class TcpWireListener;

class TcpWireStream final
{
public:
    TcpWireStream();
    ~TcpWireStream();

    TcpWireStream(TcpWireStream&&) noexcept;
    TcpWireStream& operator=(TcpWireStream&&) noexcept;

    TcpWireStream(const TcpWireStream&) = delete;
    TcpWireStream& operator=(const TcpWireStream&) = delete;

    bool connect(
        const std::string& host,
        std::uint16_t port
    );

    void service();
    bool send(
        WireMessageKind kind,
        std::vector<std::uint8_t> payload
    );
    bool receive(WireFrame& outFrame);

    bool connected() const noexcept;
    void close();

    const std::string& lastError() const noexcept;
    std::size_t queuedWriteBytes() const noexcept;
    std::uint64_t lastInboundSequence() const noexcept;
    std::uint64_t lastOutboundSequence() const noexcept;

private:
    struct Impl;
    explicit TcpWireStream(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> m_impl;

    friend class TcpWireListener;
};

class TcpWireListener final
{
public:
    TcpWireListener();
    ~TcpWireListener();

    TcpWireListener(TcpWireListener&&) noexcept;
    TcpWireListener& operator=(TcpWireListener&&) noexcept;

    TcpWireListener(const TcpWireListener&) = delete;
    TcpWireListener& operator=(const TcpWireListener&) = delete;

    bool listen(
        const std::string& bindAddress,
        std::uint16_t port
    );

    std::unique_ptr<TcpWireStream> acceptPending();

    bool listening() const noexcept;
    std::uint16_t localPort() const noexcept;
    void close();

    const std::string& lastError() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace game::network::wire
