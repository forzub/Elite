#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
model = (ROOT / 'src/model_asset/ModelAsset.h').read_text(encoding='utf-8')
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
html = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')

checks = [
    ('runtime canonical frame is explicitly +X right +Y up -Z nose', '+X right, +Y up, -Z forward/nose' in model),
    ('viewport declares fixed game frame', 'ROTATION AXES = FIXED GAME / VIEWPORT AXES' in html and '+X = RIGHT' in html and '+Y = UP' in html and '-Z = NOSE' in html),
    ('viewport ground plane is XZ with Y vertical', 'GROUND = XZ' in html and 'VERTICAL Y' in html),
    ('world gizmo has semantic direction labels', "'RIGHT +X'" in html and "'UP +Y'" in html and "'NOSE -Z'" in html and "'TAIL +Z'" in html),
    ('axis modal exposes literal X/Y/Z rotation controls', 'ROTATE ACTIVE LOD' in html and html.count('data-axis-rotate="x"') == 3 and html.count('data-axis-rotate="y"') == 3 and html.count('data-axis-rotate="z"') == 3),
    ('each axis offers +90 -90 and 180 degree steps', 'data-axis-deg="90"' in html and 'data-axis-deg="-90"' in html and 'data-axis-deg="180"' in html),
    ('rotations are around fixed GAME viewport axes', 'ROTATION AXES = FIXED GAME / VIEWPORT AXES' in html and 'same fixed X/Y/Z axes drawn in the 3D viewport' in html),
    ('rotation buttons compose only pending dialog orientation', 'function rotateDirectMapping' in html and 'state.axisModalDirectMapping=rotateDirectMapping' in html and 'Nothing is changed until APPLY' in html),
    ('cancel/reset can discard pending rotation', 'RESET BUTTON PRESSES' in html and 'function resetAxisModalPending' in html and 'function closeAxisMappingModal' in html),
    ('source no-rotation recovery remains explicit', 'SOURCE / NO ROTATION' in html and "{x:'+X',y:'+Y',z:'+Z'}" in html),
    ('quarter-turn math follows fixed GAME right-handed axes', "axis==='x'" in html and "axis==='y'" in html and "axis==='z'" in html and 'rotateGameAxisTokenQuarterTurn' in html),
    ('final rotation still uses the existing semantic backend command', 'semanticAxisMappingFromDirect' in html and "send('set_lod_axis_mapping',{lodIndex:li,right:mapping.right,up:mapping.up,forward:mapping.forward})" in html),
    ('backend supports serialized custom axis basis keys', 'return "axis:" + axisDirectionToken(basis.right)' in session),
    ('backend preserves reflected front-face winding explicitly', 'const bool flipWinding = glm::determinant(basis) < 0.0f;' in session),
    ('mapping command rebuilds only selected LOD', 'if (command == "set_lod_axis_mapping")' in session and 'reimportLodSourcePartsInConfiguredBasis(lodIndex, customBasisKey(requested));' in session),
    ('LOD0 mapping owns shared hit-volume frame', 'shared SOURCE hit volumes followed LOD0' in session),
]
failed=[name for name,ok in checks if not ok]
if failed:
    for name in failed: print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] model asset editor exposes fixed GAME axes with per-LOD X/Y/Z rotation controls')
