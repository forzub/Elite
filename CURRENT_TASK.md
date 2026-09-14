# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** Game runtime compile-time decomposition
**Wave:** R0 — seed deterministic navigation library

## Immediate goal

Establish one real, behavior-preserving game-runtime library boundary before attempting broader decomposition:

- create `EliteNavigationGeometry` as a STATIC library;
- move `NavigationObstacleGeometry.cpp` and `GeometricPathPlanner.cpp` out of both executable source lists;
- link the same library from `EliteGame` and `EliteServer`;
- enforce that the seed library cannot depend on render/UI/window/client/server/platform effects;
- keep gameplay/API behavior unchanged;
- pass a headless server build plus the new architecture contract;
- close the discovered headless CPU-assembly ownership gap through `EliteAssemblyGeometry`;
- require local MinGW client + headless-server builds before Wave R0 is accepted.

## After R0 acceptance

Wave R1 expands the navigation functional core only after dependency tracing. Candidate modules include smoothing, trajectory generation/prediction, docking and guidance mathematics. Stateful client navigation workspace, presentation, renderer and input stay outside the pure/deterministic core.

## Required state discipline

Every completed runtime-decomposition wave must update `CURRENT_STATE.md`, `CURRENT_TASK.md` and `src/game/GAME_RUNTIME_DECOMPOSITION.md` in the same commit. Do not leave architectural intent only in chat history.
