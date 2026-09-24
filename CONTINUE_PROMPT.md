# CONTINUE PROMPT — fix post-publish dock-axis prediction mismatch

Continue in GitHub repository `forzub/Elite`, branch `main`.

Read `AGENTS.md`, `CURRENT_STATE.md`, `CURRENT_TASK.md`,
`PROJECT_STATE.md`, and the latest dated section of
`src/game/navigation/STAGE12_END_TO_END.md`.

Latest target game result:

```text
route appears briefly
[DockAdvisory] request=1 failed=dock moved off approach axis
[DockAdvisory] request=2 failed=dock moved off approach axis
```

Current code ordering means route visibility already proves:
- server Autopilot ownership was observed;
- Hub-relative linear speed settled;
- pitch/yaw/roll rate settled;
- a fresh authoritative planning snapshot was captured;
- DockingAdvisoryPlanner returned a valid plan;
- Hub/HUD guidance was published.

The current failure occurs afterward. SpaceState compares:
`frame.localToWorldPosition(active.gates.back().positionMeters)`
against
`predictHubSemanticAnchorAt(active.port, time).positionMeters +
 port.forward() * active.standoffMeters`
with a 2 m tolerance.

The diagnostic far dock is yawed/spinning. Investigate whether the fixed
Hub-local gate and semantic anchor are being advanced through different
kinematic models/epochs. First add focused numeric logging for both predicted
states and delta. Then unify prediction ownership; do not simply widen the
threshold.

Also capture authoritative hand-back evidence. Publication sends Complete, but
the supplied fragment did not show:
`[DockPrep] published ... human_restored=1`
and
`[DockAdvisory] ... phase=manual human_control=1`.

Keep current requirements: physical stop, fresh stopped snapshot, fixed nominal
500 m gates, localized blinking manual docking status, authoritative Human
hand-back, card-close reset, post-entry corridor reset, automatic DOCKING
disabled.

Synchronize MD state/task/project/Stage-12/prompt after each state-affecting
result. Never record hand-back as target-proven without the server/client
confirmation lines.
