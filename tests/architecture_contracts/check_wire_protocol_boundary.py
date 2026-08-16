#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] wire protocol boundary: {message}", file=sys.stderr)
    raise SystemExit(1)


wire = read("src/game/network/WireProtocol.h")
runner = read("src/game/server/ServerRunner.cpp")
runtime = read("src/game/server/ServerRuntime.cpp")

for token in (
    "WireMagic",
    "WireProtocolVersion",
    "MaxWirePayloadBytes",
    "MaxWireStringBytes",
    "WireHeaderBytes",
    "enum class WireMessageKind",
    "class WireFrameDecoder",
    "wire protocol version mismatch",
    "wire payload exceeds protocol limit",
    "encodeSessionHello(",
    "decodeSessionHello(",
    "encodeSessionReject(",
    "decodeSessionReject(",
    "SessionReject = 9",
    "encodeSessionWelcome(",
    "decodeSessionWelcome(",
    "encodeClientMessage(",
    "decodeClientMessage(",
    "encodeTimeSyncRequest(",
    "decodeTimeSyncRequest(",
    "encodeTimeSyncResponse(",
    "decodeTimeSyncResponse(",
    "encodeMapRequest(",
    "decodeMapRequest(",
):
    if token not in wire:
        fail(f"portable wire seam is incomplete: {token}")

for forbidden in (
    "#include <windows.h>",
    "#include <winsock2.h>",
    "#include <sys/socket.h>",
    "SOCKET ",
    "WSAStartup",
    "send(",
    "recv(",
):
    if forbidden in wire:
        fail(f"platform/socket API leaked into portable codec: {forbidden}")

# The transport format must never depend on the compiler's aggregate layout.
for forbidden in (
    "sizeof(ClientMessage)",
    "sizeof(SessionWelcome)",
    "sizeof(MapRequest)",
    "reinterpret_cast<const std::uint8_t*>(&value)",
    "reinterpret_cast<const char*>(&value)",
):
    if forbidden in wire:
        fail(f"ABI-dependent raw aggregate serialization detected: {forbidden}")

# Server authority layers remain platform-neutral while process transport is
# being added underneath the existing endpoint interfaces.
for text, label in ((runner, "ServerRunner.cpp"), (runtime, "ServerRuntime.cpp")):
    for forbidden in (
        "#include <windows.h>",
        "#include <winsock2.h>",
        "#include <sys/socket.h>",
        "WSAStartup",
        "asio::ip::tcp",
    ):
        if forbidden in text:
            fail(f"network platform detail leaked into {label}: {forbidden}")

# M8A is deliberately control-plane only. Do not accidentally claim remote
# gameplay transport before snapshot/map-response codecs exist.
for token in (
    "SimulationSnapshot = 6",
    "MapResponse = 7",
    "Stage M8A covers the connection/control plane",
):
    if token not in wire:
        fail(f"M8A scope/version fence missing: {token}")

print("[PASS] platform-neutral versioned wire protocol boundary")
