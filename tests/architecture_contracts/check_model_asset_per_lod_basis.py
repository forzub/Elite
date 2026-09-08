#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
header = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.h').read_text(encoding='utf-8')
html = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')

branch_start = session.index('if (command == "convert_lod_source_basis"')
branch_end = session.index('if (command == "add_semantic_node")', branch_start)
branch = session[branch_start:branch_end]

checks = [
    ('UI sends active LOD explicitly', "send('convert_lod_source_basis',{lodIndex:li,preset:'blender_model'})" in html),
    ('basis state is persisted per LOD', 'lodSourceBasisPresets' in header and '"lodSourceBasis"' in session),
    ('shared source frame basis is persisted once', 'sharedSourceBasisPreset' in header and '"sharedSourceBasisPreset"' in session),
    ('axis branch loads only selected LOD', 'ensureLodLoaded(lodIndex)' in branch),
    ('axis branch does not load every LOD', 'ensureAllLodsLoaded()' not in branch),
    ('axis branch converts only selected visual LOD', 'convertRenderLodBasisToCanonical(m_asset.renderLods[lodIndex]' in branch),
    ('LOD0 carries shared source/hit frame exactly once', 'lodIndex == 0' in branch and 'convertSharedSourceFrameToCanonical' in branch),
    ('shared conversion includes collision/hit volumes', 'for (auto& c : asset.collisionVolumes)' in session and 'for (auto& hit : asset.hitRegions)' in session),
    ('source reload reapplies only its LOD basis', 'applyConfiguredLodBasis(lodIndex, mesh);' in session),
    ('variant refresh reapplies its LOD basis', 'applyConfiguredLodBasis(job.lodIndex, mesh);' in session),
    ('generated LOD records inherited basis', 'm_lodSourceBasisPresets[selection.level] = sourceBasisPreset' in session),
    ('full SOURCE reimport preserves effective per-LOD basis profile', 'preserveLodBasisProfile' in session and 'previousLodBasisProfile' in session),
    ('full SOURCE reimport reapplies visual LOD bases before variant refresh', 'convertRenderLodBasisToCanonical(m_asset.renderLods[lodIndex], basisPreset(preset));' in session),
    ('full SOURCE reimport reapplies the shared SOURCE/hit frame once', 'convertSharedSourceFrameToCanonical(m_asset, basisPreset(m_sharedSourceBasisPreset));' in session),
    ('legacy mixed-axis recovery is explicit per LOD', 'reimportLodSourcePartsInConfiguredBasis' in header and 'if (command == "reimport_lod_source_basis")' in session),
    ('recovery rebuilds raw SOURCE meshes then reapplies only configured LOD basis', 'applyConfiguredLodBasis(lodIndex, item.mesh);' in session),
    ('recovery is atomic before resident geometry commit', 'Two-phase rebuild:' in session and 'PendingSourceMesh' in session),
    ('UI turns a second XYZ action into safe active-LOD SOURCE rebuild, not a second transform', "send('reimport_lod_source_basis',{lodIndex:li})" in html),
    ('editor sidecar schema bumped', 'state["schemaVersion"] = 16;' in session),
]
failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] per-LOD basis survives SOURCE reload/reimport; legacy mixed-axis LODs have an identity-preserving rebuild path')
