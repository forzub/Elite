# Client acceptance harness

Run from the repository root under MSYS2 MinGW64:

```bash
bash tests/client_acceptance/run_mingw64.sh
```

The harness boots the real in-process `LocalGameSession` without creating a
window. It deliberately drives production client/server paths instead of
mutating simulation objects directly.

The current coverage inventory is maintained in `CAPABILITIES.md`; it also
marks client behavior that is visible in the game but is **not yet** protected
by an automated gate.

Current acceptance scenarios:

- keyboard semantics through `PlayerInputMapper` using a synthetic key source;
- local session synchronization and player startup invariants;
- idle stability in the canonical HubTactical reference frame;
- accelerated universe-time entry/exit, including rejection of controls issued
  while the production gameplay branch is frozen;
- yaw/orientation basis integrity and forward manoeuvre motion after rotation;
- fixed-step client command acknowledgement by the authoritative server;
- remote Hub Motion Lab NPC movement through `ShipSnapshot -> ClientWorldState`
  presentation;
- live Galaxy -> System -> Details -> Hub data requests through
  `GameClient -> LocalLoopbackTransport -> GameServer -> ClientMapService`;
- universe-timeline revision parity between gameplay and all accepted map
  snapshots.

This is intentionally not a framebuffer/golden-image test. Map camera,
selection, picking and cubic-navigation behavior remain covered by
`tests/system_map`; this harness verifies the live client/server data path that
feeds those views.

When a user-visible regression is found, add the smallest reproduction here if
it crosses multiple runtime layers. Pure math/data contracts should remain in
their narrower test suites.
