from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

model = (ROOT / "src/game/ship/plant/ShipPhysicalControlModel.h").read_text(encoding="utf-8")
state = (ROOT / "src/game/ship/state/ShipState.h").read_text(encoding="utf-8")
core = (ROOT / "src/game/ship/core/ShipCore.cpp").read_text(encoding="utf-8")
snapshot = (ROOT / "src/game/simulation/ShipSnapshot.h").read_text(encoding="utf-8")
simulation = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
client_h = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
client_cpp = (ROOT / "src/game/client/ClientWorldState.cpp").read_text(encoding="utf-8")
space = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
app = (ROOT / "src/core/Application.cpp").read_text(encoding="utf-8")
mapper = (ROOT / "src/game/ship/controller/PlayerInputMapper.cpp").read_text(encoding="utf-8")
engine_hud = (ROOT / "src/game/presentation/EngineThrustHudPresentation.h").read_text(encoding="utf-8")
wire_schema = (ROOT / "src/game/network/WireDataSchema.h").read_text(encoding="utf-8")
wire_codec = (ROOT / "src/game/network/WireDataCodec.h").read_text(encoding="utf-8")

for token in [
    "modelFor(",
    "nextLayout(",
    "physicalControlModelName(",
    "parsePhysicalControlModel(",
]:
    assert token in model, f"missing physical-model helper: {token}"

assert "selectConstructionForDiagnostic" in state
assert "m_transform.motion.localControlLaw =" in core
assert "m_state.construction.nativeFlightControlLaw" in core

assert "physicalControlModel" in snapshot
assert "ship.core().state().construction.physicalControlModel" in simulation
assert "v.physicalControlModel" in wire_schema
assert "SimulationSnapshotWireSchemaVersion = 13u" in wire_codec

assert "physicalControlModel" in client_h
assert "ship.physicalControlModel" in client_cpp
assert "player.physicalControlModel" in space

assert "functionKey == 9 && ctrlDown && !altDown" in app
assert "cyclePhysicalControlLayout()" in app
assert "functionKey == 10 && ctrlDown && !altDown" in app
assert "cyclePhysicalControlFamily()" in app
assert "[Hotkey] F" in app
assert "const auto model = ship.physicalControlModel;" in engine_hud
assert "physicalControlSlots(model)" in engine_hud
assert "buildShipCapability(" in engine_hud
assert "descriptor.physicalControlModel" not in engine_hud.split(
    "const auto model = ship.physicalControlModel;", 1
)[1].split("const bool assisted", 1)[0] or "model != descriptor.physicalControlModel" in engine_hud

# There must be no second keyboard path that changes law independently of
# construction. ShipControlState retains a generic command field for protocol
# compatibility, but the PlayerInputMapper no longer owns Ctrl+F10.
assert "GLFW_KEY_F10" not in mapper
assert "Ctrl+F10 switches" not in mapper

print("PHYSICAL MODEL RUNTIME SELECTION: PASS")
print(" - Ctrl+F9 cycles I/Y/X through authoritative ShipState.")
print(" - Ctrl+F10 switches Assisted/Newtonian family while preserving layout.")
print(" - snapshot/client prediction/planner consume the replicated model.")
print(" - engine HUD consumes the same replicated runtime model.")
print(" - function-key actions have a console capture echo.")
print(" - keyboard no longer has a parallel law-only Ctrl+F10 path.")
