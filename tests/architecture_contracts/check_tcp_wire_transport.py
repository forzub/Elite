#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] TCP wire transport boundary: {message}", file=sys.stderr)
    raise SystemExit(1)


stream_h = read("src/game/network/TcpWireStream.h")
stream_cpp = read("src/game/network/TcpWireStream.cpp")
transport_h = read("src/game/network/TcpTransport.h")
transport_cpp = read("src/game/network/TcpTransport.cpp")
message_codec = read("src/game/network/WireMessageCodec.h")
runner = read("src/game/server/ServerRunner.cpp")
runtime = read("src/game/server/ServerRuntime.cpp")

for token in (
    "class TcpWireStream",
    "class TcpWireListener",
    "MaxTcpQueuedWireBytes",
    "WireMessageKind kind",
    "WireFrame& outFrame",
):
    if token not in stream_h:
        fail(f"schema-blind stream seam is incomplete: {token}")

for token in (
    "#include <asio.hpp>",
    "socket.non_blocking(true",
    "Tcp::no_delay(true)",
    "WireFrameDecoder",
    "frame.sequence != expected",
    "TCP wire write queue exceeded safety limit",
):
    if token not in stream_cpp:
        fail(f"TCP stream implementation is incomplete: {token}")

# The actual socket stream must stay blind to game/schema/compression content.
for forbidden in (
    "SimulationSnapshot",
    "ShipSnapshot",
    "ObjectSnapshot",
    "MapResponse",
    "ClientMessage",
    "IWireCompressor",
    "WireDataSchema",
    "shipCount",
    "moduleCount",
):
    if forbidden in stream_h or forbidden in stream_cpp:
        fail(f"TCP byte stream learned gameplay/schema detail: {forbidden}")

for token in (
    "class TcpClientTransport final : public ITransport",
    "class TcpServerTransport final : public IServerTransport",
    "class TcpServerListener",
):
    if token not in transport_h:
        fail(f"typed TCP adapter seam is incomplete: {token}")

for token in (
    "encodeMessagePayload(",
    "decodeMessagePayload(",
    "encodeCompressedDataPlanePayload(",
    "decodeCompressedDataPlanePayload(",
):
    if token not in message_codec:
        fail(f"wire message bridge is incomplete: {token}")

# Compression remains byte-to-byte and below logical serialization. TCP should
# use the bridge rather than reaching into snapshot fields or schema tuples.
for forbidden in (
    "shipCoreStatus",
    "removedShipIds",
    "playerNavigation",
    "ObjectGraphSnapshot",
    "ELITE_WIRE_SCHEMA",
):
    if forbidden in transport_cpp:
        fail(f"typed TCP adapter reached inside replicated data: {forbidden}")

# Asio/OS network details must not leak upward into authoritative gameplay.
for text, label in ((runner, "ServerRunner.cpp"), (runtime, "ServerRuntime.cpp")):
    for forbidden in (
        "#include <asio.hpp>",
        "asio::ip::tcp",
        "#include <winsock2.h>",
        "#include <sys/socket.h>",
        "SOCKET ",
    ):
        if forbidden in text:
            fail(f"socket implementation leaked into {label}: {forbidden}")

# Keep Asio hidden from public protocol/transport interfaces so the same game
# authority code can build against another adapter later.
for text, label in (
    (stream_h, "TcpWireStream.h"),
    (transport_h, "TcpTransport.h"),
):
    if "#include <asio" in text or "asio::" in text:
        fail(f"Asio leaked through public adapter header: {label}")

print("[PASS] Asio TCP is confined below framed byte/message transport boundaries")
