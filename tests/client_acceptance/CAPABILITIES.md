# Client capability regression matrix

This file is the human-readable inventory for client mechanics that are treated
as stable enough to protect. A capability moves to **protected** only when a
regression gate exercises its production path.

| Capability | Current runtime interpretation | Regression owner | State |
|---|---|---|---|
| Local session boot/sync | Real `LocalGameSession` reaches gameplay-ready client state | client acceptance | protected |
| Player system/reference frame | Player starts in the authoritative active system with a valid hub frame | client acceptance | protected |
| Keyboard -> ship control | W/S, A/D, Q/E, target-speed keys, keypad manoeuvre thrusters, J cruise gating | client acceptance | protected |
| Player attitude | Production fixed-step control changes orientation; forward/right/up remain orthonormal and handed correctly | client acceptance | protected |
| Player HubTactical motion | Client command -> server acknowledgement -> authoritative hub-local movement | client acceptance | protected |
| Orientation drives thrust direction | After yaw, forward manoeuvre movement must remain on the ship's forward side | client acceptance | protected |
| Idle stability | No-command player must not drift or rotate in canonical hub-local state | client acceptance | protected |
| Accelerated universe-time diagnostic | Enter/exit revision fence; controls touched while frozen must not leak back into gameplay | client acceptance + world runtime | protected |
| Remote NPC presentation | Hub Motion Lab ships move through authoritative snapshot/interpolation path | client acceptance + presentation pipeline | protected |
| Galaxy map data | Live client/server request and timeline-consistent snapshot | client acceptance | protected |
| System map data | Live request for current system and selectable hub inventory | client acceptance | protected |
| Details map data | Semantic Details target survives live request/response path | client acceptance | protected |
| Hub map data | Selected hub survives live request/response path | client acceptance | protected |
| Map camera/grid/picking | Camera, cubic navigation, picking and presentation contracts | system_map | protected |
| Actual WebView map button dispatch | Browser command -> `Application` -> `SpaceState` transition | none yet | not protected |
| HUD values visible on framebuffer | Rendered text/value parity with client state | none yet | not protected |
| Radar widget output | Contact data -> selected radar presentation -> framebuffer | partial subsystem tests only | not protected |
| Framebuffer/shader smoke | A valid gameplay/map frame is actually produced by OpenGL | none yet | not protected |
| Jump flight mode | `jumpActive` exists in control state but has no current keyboard mapping in `PlayerInputMapper` | none | not claimed working |
| Cruise as a runtime motion mode | J currently gates controls via `cruiseActive`; initial player runtime mode remains `HubTactical` | input acceptance | control flag only |

When a protected row regresses, add or tighten the smallest failing scenario
before fixing the production code. That turns each discovered corpse into a
future tripwire.
