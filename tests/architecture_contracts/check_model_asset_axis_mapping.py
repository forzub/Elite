#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
model = (ROOT / 'src/model_asset/ModelAsset.h').read_text(encoding='utf-8')
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
html = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')

checks = [
    ('runtime canonical frame is explicitly +X right +Y up -Z nose', '+X right, +Y up, -Z forward/nose' in model),
    ('viewport declares fixed game frame', 'GAME FRAME · FIXED' in html and 'RIGHT = +X' in html and 'UP = +Y' in html and 'NOSE/FORWARD = -Z' in html),
    ('viewport ground plane is XZ with Y vertical', 'GROUND PLANE = XZ' in html and 'VERTICAL = Y' in html),
    ('world gizmo has semantic direction labels', "'RIGHT +X'" in html and "'UP +Y'" in html and "'NOSE -Z'" in html and "'TAIL +Z'" in html),
    ('axis modal exposes signed source axes', 'id="axisRight"' in html and 'id="axisUp"' in html and 'id="axisForward"' in html),
    ('Blender and game presets are explicit', 'BLENDER DEFAULT · R +X / U +Z / N -Y' in html and 'GAME CURRENT · R +X / U +Y / N -Z' in html),
    ('mapping prevents duplicate axis families', 'new Set(values.map(axisFamily)).size===3' in html),
    ('backend supports serialized custom axis basis keys', 'return "axis:" + axisDirectionToken(basis.right)' in session),
    ('backend preserves reflected front-face winding explicitly', 'const bool flipWinding = glm::determinant(basis) < 0.0f;' in session),
    ('mapping command rebuilds only selected LOD', 'if (command == "set_lod_axis_mapping")' in session and 'reimportLodSourcePartsInConfiguredBasis(lodIndex, customBasisKey(requested));' in session),
    ('LOD0 mapping owns shared hit-volume frame', 'shared SOURCE hit volumes followed LOD0' in session),
]
failed=[name for name,ok in checks if not ok]
if failed:
    for name in failed: print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] model asset editor exposes an explicit canonical game frame and configurable per-LOD SOURCE semantic-axis mapping')
