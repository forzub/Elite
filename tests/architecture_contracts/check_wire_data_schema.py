#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] wire data schema: {message}", file=sys.stderr)
    raise SystemExit(1)


binary = read("src/game/network/WireBinaryCodec.h")
schema = read("src/game/network/WireDataSchema.h")
data_codec = read("src/game/network/WireDataCodec.h")
compression = read("src/game/network/WireCompression.h")
wire = read("src/game/network/WireProtocol.h")

for token in (
    "struct WireSchema",
    "MaxWireContainerElements",
    "WireSchema<T>::fields",
    "IsVector",
    "IsVariant",
    "No binary wire codec/schema for this type",
    "Do not reserve an untrusted network count",
):
    if token not in binary:
        fail(f"generic binaryizer seam is incomplete: {token}")

for token in (
    "CANONICAL DATA-PLANE WIRE SCHEMA",
    "ELITE_WIRE_SCHEMA(",
    "SimulationSnapshot,",
    "ShipSnapshot,",
    "ObjectSnapshot,",
    "game::simulation::OrbitalHubSnapshot,",
    "game::network::GalaxyMapResponse,",
    "v.replication",
    "v.graph",
    "v.session",
    "game::radar::RadarScanReport,",
    "game::navigation::NavigationSolution,",
    "game::simulation::ClientNavigationSensorSnapshot,",
    "v.navigationSensors",
):
    if token not in schema:
        fail(f"canonical ordered schema is incomplete: {token}")

for token in (
    "game::network::SystemMapResponse",
    "game::network::DetailMapResponse",
    "game::network::HubMapResponse",
):
    if token in schema:
        fail(f"obsolete client-composed map response remains in wire schema: {token}")

for token in (
    "SimulationSnapshotWireSchemaVersion",
    "MapResponseWireSchemaVersion",
    "encodeSimulationSnapshot(",
    "decodeSimulationSnapshot(",
    "encodeMapResponse(",
    "decodeMapResponse(",
    "binary::encodeValue(writer, value)",
    "binary::decodeValue(reader, outValue)",
):
    if token not in data_codec:
        fail(f"top-level data-plane codec is incomplete: {token}")

# The top-level serializer must delegate field order to the schema rather than
# becoming another monolithic hand-written list.
for forbidden in (
    "value.ships",
    "value.objects",
    "value.hubs",
    "value.shipCoreStatus",
    "value.modules",
    "value.damageEvents",
):
    if forbidden in data_codec:
        fail(f"top-level codec manually knows nested world fields: {forbidden}")

for token in (
    "class IWireCompressor",
    "class NoWireCompression",
    "const std::vector<std::uint8_t>& input",
    "CompressedWirePayload",
):
    if token not in compression:
        fail(f"opaque compression boundary is incomplete: {token}")

# Compression is byte-to-byte only. It must never grow knowledge of gameplay
# structs/counts; adding a replicated field should not touch this layer.
for forbidden in (
    "SimulationSnapshot",
    "ShipSnapshot",
    "ObjectSnapshot",
    "MapResponse",
    "EntityId",
    "shipCount",
    "moduleCount",
):
    if forbidden in compression:
        fail(f"compressor leaked gameplay/schema knowledge: {forbidden}")

# Framing also remains ignorant of data-plane field order.
for forbidden in (
    "shipCoreStatus",
    "removedShipIds",
    "ObjectGraphSnapshot",
    "playerNavigation",
):
    if forbidden in wire:
        fail(f"wire framing leaked data-plane field knowledge: {forbidden}")

for forbidden in (
    "#include <windows.h>",
    "#include <winsock2.h>",
    "#include <sys/socket.h>",
    "asio::ip::tcp",
):
    for text, label in (
        (binary, "WireBinaryCodec.h"),
        (schema, "WireDataSchema.h"),
        (data_codec, "WireDataCodec.h"),
        (compression, "WireCompression.h"),
    ):
        if forbidden in text:
            fail(f"platform/socket API leaked into {label}: {forbidden}")

print("[PASS] one ordered data schema -> bytes -> schema-blind compression boundary")
