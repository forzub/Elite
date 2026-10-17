# BITE library boundary

BITE is a diagnostic/presentation library. It does not own equipment damage,
ship physics, control, repair, or AI state.

## Hard API boundary

Allowed input:
- `game::bite::EquipmentSample`
- source revision

Allowed output:
- `game::bite::State`
- `game::bite::terminalLines(State)`

Files inside `src/game/bite` must not include ship runtime, module runtime,
client world state, HUD/render, networking, damage handling, or equipment
implementation headers. Callers adapt authoritative equipment state into the
small input DTO. BITE never reaches back across the boundary to fetch data.

HUD/render code consumes BITE output through its own presentation adapter; it
must not read BITE internals or duplicate fault thresholds.

BITE is never an authority for physics. Deleting this library must not change
ship dynamics.
