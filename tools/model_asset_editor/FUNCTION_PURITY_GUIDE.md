# Model Asset Editor function-purity migration

This harness prepares the legacy v0.10.66 WebUI for a later module split without changing editor behaviour first. It deliberately separates **static purity protection** from **dynamic behavioural equivalence**.

## Layer 1 — static call-graph contract

`FUNCTION_PURITY_STATIC_BASELINE.json` inventories all 420 named `function ...` declarations in the v0.10.66 WebUI and protects the low-risk subset.

The analyser lexes JavaScript strings, comments, regex literals and template-literal expressions, builds a named-function call graph, detects direct side effects, propagates them transitively, follows common aliases from arguments / mutable module globals, and classifies each function as one of:

- `PURE` — no hidden mutable-global read and no direct/transitive hard side effect.
- `EASY_CANDIDATE` — no hard side effect; reads mutable module state directly and can normally become pure by making those dependencies explicit arguments.
- `TRANSITIVE_CANDIDATE` — no hard side effect; hidden mutable state arrives through helper calls. Purification requires passing the dependency through that call chain.
- `STATE_MUTATING`, `GLOBAL_MUTATING`, `ARG_MUTATING`, `DOM_UI`, `COMMAND`, `STATUS_IO`, `IO`, `EVENT_LOOP`, `NONDETERMINISTIC`, `COMPOSITE`, or `UNKNOWN` — deferred for now.

The baseline is monotonic:

- baseline `PURE` must remain `PURE`;
- baseline `EASY_CANDIDATE` may remain `EASY_CANDIDATE` or improve to `PURE`;
- baseline `TRANSITIVE_CANDIDATE` may remain low-risk or improve to `EASY_CANDIDATE` / `PURE`;
- new hidden mutable-global dependencies are forbidden;
- added/removed named functions require explicit baseline review instead of silently changing the census.

At the v0.10.66 baseline the protected static surface is:

- 53 `PURE`;
- 32 `EASY_CANDIDATE`;
- 81 `TRANSITIVE_CANDIDATE`;
- 254 deferred/hard.

After migration wave 1, without changing the frozen static baseline itself, the current graph improves to:

- 60 `PURE`;
- 32 `EASY_CANDIDATE`;
- 74 `TRANSITIVE_CANDIDATE`;
- 254 deferred/hard.

Dynamic frozen v0.10.66 equivalence now covers 56 functions: 55 current `PURE` and one remaining contracted candidate. Five additional statically pure helpers that construct or calculate through `THREE` remain static-only until a dedicated Three.js runtime adapter is reviewed.

This static protection is broader than the dynamic fixture set, but it is intentionally conservative. Class methods, anonymous/arrow functions and unresolved external behaviour are not silently assumed pure; suspicious paths remain deferred.

## Layer 2 — frozen behavioural equivalence

`FUNCTION_PURITY_CONTRACT.json` contains the functions currently being actively migrated. Each fixture stores v0.10.66 inputs plus the expected output. Contract schema 2 additionally stores an `oracle_sha256` per certified function. This matters during incremental expansion: adding fixtures for a new function may legitimately change the global contract hash, but it cannot silently re-baseline an already certified function.

At runtime the test:

- rebuilds the exact fixture state/arguments;
- deep-freezes fixture inputs;
- snapshots inputs before/after the call to catch mutation;
- calls the function twice on the same frozen input to check determinism;
- deep-compares the current result with the frozen v0.10.66 result.

Therefore a migration may change the function signature or implementation, but not its observable calculation result for the reviewed fixtures.

## Migration rule

For a candidate, first add reviewed fixtures to the dynamic contract **before changing production code**. Then replace hidden state reads with explicit arguments.

Example:

```js
// legacy
function semanticRootIndices() {
  const nodes = state.asset?.nodes || [];
  ...
}

// purified
function semanticRootIndices(nodes) {
  ...
}
```

The fixture `invoke` adapter may change to call the new signature. `invoke` and the `pure` / `easy_candidates` grouping are excluded from the behavioural baseline hash, so signature cleanup cannot rewrite the old input/output oracle.

After production call sites no longer require hidden mutable state and the call graph classifies the implementation as `PURE`, move the dynamically contracted entry from `easy_candidates` to `pure`.

## Commands

```bash
python tests/architecture_contracts/check_model_asset_function_purity.py
python tests/architecture_contracts/check_model_asset_function_purity.py --report
python tests/architecture_contracts/check_model_asset_function_purity.py --report-all
```

`--report` prints the protected low-risk queues. `--report-all` additionally prints deferred/hard classifications and their propagated effect sets.

On Windows/MSYS2 the runtime harness resolves Node through native PATH first and then through the exact MSYS2 `bash.exe` associated with the running Python. The Python↔Node channel is forced to strict UTF-8. For unusual Node installations, `MODEL_ASSET_EDITOR_NODE=C:\\path\\to\\node.exe` is an explicit override.

`--record-goldens` only fills missing expected outputs for newly reviewed dynamic fixtures. It refuses to overwrite existing v0.10.66 results. A function with complete expected outputs but no per-function oracle hash is rejected rather than silently adopted as a new baseline.

## Current migration strategy

Do not touch the deferred orchestration/DOM/command classes yet. The intended order is:

1. dynamically certify the existing statically `PURE` helpers that matter to the move;
2. dynamically certify and purify `EASY_CANDIDATE` functions;
3. purify `TRANSITIVE_CANDIDATE` chains from the leaves upward;
4. only after the computational core is substantially pure, split SOURCE / LOD / GEOMETRY / SURFACES / SEMANTICS into physical modules.

This keeps architecture changes separate from behavioural changes and makes the later module transfer much easier to verify.
## Migration wave 2

The second migration wave freezes and promotes direct hidden-read helpers before touching transitive chains. Current certified state: 70 functions, all PURE. Static call graph: 75 PURE / 26 EASY / 65 TRANSITIVE / 254 deferred. When a pure-signature change touches a frozen accepted tab, its fingerprint may move only under the explicit PATCH_CONTRACT purity-refactor exception and only after the old behaviour has an immutable per-function oracle.
