# Client acceptance harness

Run from the repository root under MSYS2 MinGW64:

```bash
bash tests/client_acceptance/run_mingw64.sh
```

The harness boots the real in-process `LocalGameSession` without creating a
window. It drives production client/server and presentation seams rather than
mutating simulation objects directly.

The functional inventory is maintained in `CAPABILITIES.md`. If a row is
`protected`, the project is allowed to treat that mechanic as working until the
gate says otherwise.

Current acceptance coverage:

- keyboard semantics through the production `PlayerInputMapper` mapping path;
- local-session synchronization and player startup/reference-frame invariants;
- idle stability in canonical HubTactical space;
- yaw/orientation basis integrity and forward manoeuvre motion after rotation;
- engine target-speed controls and authoritative fixed-step acknowledgement;
- accelerated universe-time entry/exit with no frozen-branch control leakage;
- remote Hub Motion Lab NPC movement through
  `ShipSnapshot -> ClientWorldState -> renderTransform`;
- HUD coordinate/speed calculation and writes to the exact production
  `main_coord_*` UIText bindings;
- F11 edge/latch and SystemMap UI toggle semantics;
- actual `system_map_panel.html` command vocabulary tied by architecture guard
  to the same parser/dispatcher exercised by the headless suite;
- command meaning for Galaxy/System/Details/Hub/select/open/close actions;
- live Galaxy -> System -> Details -> Hub data requests through
  `GameClient -> LocalLoopbackTransport -> GameServer -> ClientMapService`;
- universe-timeline revision parity between gameplay and all accepted map
  snapshots;
- map/navigation state -> production JSON panel payload, including the fields
  consumed by `window.setSystemMapPanel`.

This is intentionally a **functional**, not visual, acceptance suite. It does
not care about button color, font rendering, antialiasing, decorative layout or
pixel-perfect screenshots. Map camera/grid/picking math remains in
`tests/system_map`. OpenGL rendering bugs should receive their own narrow smoke
test only if a real regression demonstrates that state/presentation contracts
are insufficient.

Radar is not advertised as working here because both current runtime radar
feature flags are disabled. Jump is likewise not advertised as working because
there is no active keyboard mapping for `jumpActive`.

### Functional sky/navigation coverage

The final functional layer also protects the current F9 coordinate-format cycle,
current F12 constellation-overlay toggle, authored game-system names reaching
star-sky labels, and the Galaxy player marker following real player navigation
movement. The map-panel distance-to-system calculation shares the exact player
marker resolver used by the Galaxy map, so the panel cannot silently fall back
to the current system center while the ship is moving.

Accelerated universe time is intentionally **not** used as a fake interstellar
travel shortcut in acceptance: that runtime branch freezes gameplay and rolls
back on exit. Once a production interstellar/cruise motion mode exists, add the
"aim at system -> travel -> map/navigation follows" scenario against that mode.
