#!/usr/bin/env python3
"""Regression contract for same-asset whole-SOURCE reimport stage validity.

A full SOURCE reimport rebuilds the in-memory wizard shell and SOURCE payloads, but
must preserve the pre-reimport global stage history long enough to invalidate it.
Previously loadWizardState() reset every stage to NOT_STARTED before
invalidateWizardFrom("source") ran, losing COMPLETE/STALE/NEEDS_FIX history.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SESSION = (ROOT / "tools/model_asset_editor/ModelAssetEditorSession.cpp").read_text(
    encoding="utf-8", errors="replace"
)


def body_between(body: str, start: str, end: str) -> str:
    a = body.index(start)
    b = body.index(end, a + len(start))
    return body[a:b]


select = body_between(
    SESSION,
    "bool ModelAssetEditorSession::selectAsset(const std::string& id, bool forceReimport)",
    "bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)",
)

required = (
    "const bool preserveStageValidity = forceReimport && sameSelection;",
    "const StageValidityState previousStageValidity = preserveStageValidity",
    "? captureStageValidity() : StageValidityState{};",
    "loadWizardState();",
    "if (preserveStageValidity) applyStageValidity(previousStageValidity);",
    'invalidateWizardFrom("source");',
)
for token in required:
    if token not in select:
        raise AssertionError(f"same-asset SOURCE reimport stage-validity contract missing {token!r}")

if select.count("applyStageValidity(previousStageValidity)") != 1:
    raise AssertionError("pre-reimport stage validity must be restored exactly once")

capture_pos = select.index("const StageValidityState previousStageValidity")
reset_pos = select.index("loadWizardState();")
fresh_source_pos = select.index("captureCurrentSourceFingerprintBaseline();")
clear_prepare_pos = select.index("m_meshPreparationRecords.clear();", fresh_source_pos)
restore_pos = select.index("if (preserveStageValidity) applyStageValidity(previousStageValidity);")
invalidate_pos = select.index('invalidateWizardFrom("source");', restore_pos)

if not capture_pos < reset_pos:
    raise AssertionError("stage validity snapshot must be captured before loadWizardState() resets the shell")
if not fresh_source_pos < clear_prepare_pos < restore_pos < invalidate_pos:
    raise AssertionError(
        "stage validity must be restored only after fresh SOURCE import/evidence reset, "
        "immediately before SOURCE/downstream invalidation"
    )

invalidate = body_between(
    SESSION,
    "void ModelAssetEditorSession::invalidateWizardFrom(const std::string& stage)",
    "nlohmann::json ModelAssetEditorSession::serializeWizard() const",
)
for token in (
    'if (value.status == "complete" || value.status == "stale") value.status = "stale";',
    'else if (value.status == "needs_fix") value.status = "needs_fix";',
    'else value.status = "not_started";',
):
    if token not in invalidate:
        raise AssertionError(f"stage invalidation semantics changed unexpectedly: missing {token!r}")

# This hotfix repairs history loss only. It deliberately does not bypass ordered
# wizard progression: after reimport SOURCE is STALE and must be CHECKed before
# an initial-authoring LOD CHECK can proceed.
serialize = body_between(
    SESSION,
    "nlohmann::json ModelAssetEditorSession::serializeWizard() const",
    "bool ModelAssetEditorSession::validateSurfaceGeometryStage(",
)
if 'm_wizardStages.at(order[i - 1]).status == "complete"' not in serialize:
    raise AssertionError("hotfix accidentally changed ordered wizard unlock semantics")

print(
    "[PASS] same-asset SOURCE reimport preserves stage history until invalidation: "
    "COMPLETE->STALE, STALE->STALE, NEEDS_FIX preserved; ordered progression unchanged"
)
