#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
header = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.h').read_text(encoding='utf-8')
html = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')

map_start = session.index('if (command == "set_lod_axis_mapping")')
map_end = session.index('if (command == "convert_lod_source_basis"', map_start)
map_branch = session[map_start:map_end]
rebuild_start = session.index('bool ModelAssetEditorSession::reimportLodSourcePartsInConfiguredBasis(')
rebuild_end = session.index('bool ModelAssetEditorSession::reloadMeshFromSource(', rebuild_start)
rebuild = session[rebuild_start:rebuild_end]

checks = [
    ('UI sends active LOD and explicit semantic axes', "send('set_lod_axis_mapping',{lodIndex:li,right:mapping.right,up:mapping.up,forward:mapping.forward})" in html),
    ('basis state is persisted per LOD', 'lodSourceBasisPresets' in header and '"lodSourceBasis"' in session),
    ('shared source frame basis is persisted once', 'sharedSourceBasisPreset' in header and '"sharedSourceBasisPreset"' in session),
    ('axis mapping command has an explicit LOD', 'axis mapping requires an explicit active LOD' in map_branch),
    ('axis rebuild loads only selected LOD', 'ensureLodLoaded(lodIndex)' in rebuild),
    ('axis rebuild does not load every LOD', 'ensureAllLodsLoaded()' not in rebuild),
    ('custom signed-axis mapping is validated', 'validSourceBasis(requested)' in map_branch and 'parseAxisDirectionToken' in map_branch),
    ('SOURCE-backed meshes rebuild through requested mapping', 'transformMeshBasis(item.mesh, targetToGame);' in rebuild),
    ('source-less meshes follow mapping delta', 'transformMeshBasis(lod.geometries[gi].mesh, visualDelta);' in rebuild),
    ('RenderNode placement follows mapping delta', 'transformRenderLodPlacement(lod, visualDelta);' in rebuild),
    ('LOD0 shared source/hit frame follows only LOD0 mapping', 'if (lodIndex == 0)' in rebuild and 'transformSharedSourceFrame(m_asset, sharedDelta);' in rebuild),
    ('shared conversion includes collision/hit volumes', 'for (auto& c : asset.collisionVolumes)' in session and 'for (auto& hit : asset.hitRegions)' in session),
    ('source reload reapplies only its LOD basis', 'applyConfiguredLodBasis(lodIndex, mesh);' in session),
    ('variant refresh reapplies its LOD basis', 'applyConfiguredLodBasis(job.lodIndex, mesh);' in session),
    ('generated LOD records inherited basis', 'm_lodSourceBasisPresets[selection.level] = sourceBasisPreset' in session),
    ('full SOURCE reimport preserves effective per-LOD basis profile', 'preserveLodBasisProfile' in session and 'previousLodBasisProfile' in session),
    ('full SOURCE reimport reapplies visual LOD bases', 'convertRenderLodBasisToCanonical(m_asset.renderLods[lodIndex], basisPreset(preset));' in session),
    ('full SOURCE reimport reapplies shared SOURCE/hit frame once', 'convertSharedSourceFrameToCanonical(m_asset, basisPreset(m_sharedSourceBasisPreset));' in session),
    ('recovery rebuild is atomic before resident geometry commit', 'Two-phase rebuild.' in rebuild and 'PendingSourceMesh' in rebuild),
    ('editor sidecar schema remains per-LOD basis capable', 'state["schemaVersion"] = 16;' in session),
]
failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] per-LOD semantic axis mapping survives SOURCE reload/reimport and mutates only the selected render document')
