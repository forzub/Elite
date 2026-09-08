#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
model = (ROOT / 'src/model_asset/ModelAsset.h').read_text(encoding='utf-8')
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
html = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')

checks = [
    ('runtime canonical frame is explicitly +X right +Y up -Z nose', '+X right, +Y up, -Z forward/nose' in model),
    ('viewport declares fixed game frame', 'GAME AXES · FIXED DESTINATION' in html and '+X = RIGHT' in html and '+Y = UP' in html and '-Z = NOSE' in html),
    ('viewport ground plane is XZ with Y vertical', 'GROUND = XZ' in html and 'VERTICAL Y' in html),
    ('world gizmo has semantic direction labels', "'RIGHT +X'" in html and "'UP +Y'" in html and "'NOSE -Z'" in html and "'TAIL +Z'" in html),
    ('axis modal uses direct SOURCE-to-GAME rows', 'id="axisSourceX"' in html and 'id="axisSourceY"' in html and 'id="axisSourceZ"' in html and 'SOURCE AXIS' in html and 'BECOMES IN GAME' in html),
    ('opposite signed directions are shown automatically', 'OPPOSITE FOLLOWS' in html and 'oppositeAxisToken' in html and 'axisOppositeX' in html),
    ('Blender and no-remap presets are explicit in direct notation', 'BLENDER DEFAULT · X→X / Y→Z / Z→Y' in html and 'NO REMAP · X→X / Y→Y / Z→Z' in html),
    ('requested cyclic example is one-click direct mapping', "setAxisModalDirectMapping({x:'+Y',y:'+Z',z:'+X'})" in html),
    ('direct mapping prevents duplicate target axis families', 'function validDirectAxisMapping' in html and 'new Set(values.map(axisFamily)).size===3' in html),
    ('direct UI is converted to existing semantic backend command', 'semanticAxisMappingFromDirect' in html and "send('set_lod_axis_mapping',{lodIndex:li,right:mapping.right,up:mapping.up,forward:mapping.forward})" in html),
    ('UI warns when a signed permutation mirrors handedness', 'MIRROR / HANDEDNESS FLIP' in html and 'directMappingDeterminant' in html),
    ('backend supports serialized custom axis basis keys', 'return "axis:" + axisDirectionToken(basis.right)' in session),
    ('backend preserves reflected front-face winding explicitly', 'const bool flipWinding = glm::determinant(basis) < 0.0f;' in session),
    ('mapping command rebuilds only selected LOD', 'if (command == "set_lod_axis_mapping")' in session and 'reimportLodSourcePartsInConfiguredBasis(lodIndex, customBasisKey(requested));' in session),
    ('LOD0 mapping owns shared hit-volume frame', 'shared SOURCE hit volumes followed LOD0' in session),
]
failed=[name for name,ok in checks if not ok]
if failed:
    for name in failed: print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] model asset editor exposes a fixed game frame with direct per-LOD SOURCE-axis remapping UI')
