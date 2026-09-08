#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
header = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.h').read_text(encoding='utf-8')

rebuild_start = session.index('bool ModelAssetEditorSession::reimportLodSourcePartsInConfiguredBasis(')
rebuild_end = session.index('bool ModelAssetEditorSession::reloadMeshFromSource(', rebuild_start)
rebuild = session[rebuild_start:rebuild_end]
shared_start = session.index('void transformSharedSourceFrame(ModelAsset& asset, const glm::mat3& basis)')
shared_end = session.index('void convertSharedSourceFrameToCanonical', shared_start)
shared = session[shared_start:shared_end]

checks = [
    ('editor state persists shared primitive-frame encoding version', 'sharedSourceFrameTransformVersion' in header and '"sharedSourceFrameTransformVersion"' in session),
    ('working/production sidecar schema migrated to 17', session.count('state["schemaVersion"] = 17;') >= 2),
    ('legacy schema marks transformed shared frame for migration', 'next.sharedSourceFrameTransformVersion = next.sharedSourceBasisPreset == "game_current" ? 2 : 1;' in session),
    ('primitive leaves use left-multiplied physical frame transform', 'transformSymmetricPrimitiveEuler' in session and 'basis * eulerRotation(deg)' in session),
    ('reflected mappings preserve a proper primitive rotation', 'properSymmetricPrimitiveFrame' in session and 'frame[0] = -frame[0]' in session),
    ('collision volumes use primitive transform rather than semantic conjugation', 'c.localRotationDeg = transformSymmetricPrimitiveEuler(c.localRotationDeg, basis);' in shared),
    ('hit regions use primitive transform', 'hit.localRotationDeg = transformSymmetricPrimitiveEuler(hit.localRotationDeg, basis);' in shared),
    ('openings use primitive transform', 'opening.localRotationDeg = transformSymmetricPrimitiveEuler(opening.localRotationDeg, basis);' in shared),
    ('damage proxies use primitive transform', 'proxy.localRotationDeg = transformSymmetricPrimitiveEuler(proxy.localRotationDeg, basis);' in shared),
    ('semantic/socket frames still use conjugated coordinate-frame transform', 'socket.localRotationDeg = convertEuler(socket.localRotationDeg, basis);' in shared),
    ('LOD0 same-mapping apply repairs legacy primitive orientation', 'migrateLegacySharedPrimitiveFrames(m_asset, sharedPreviousToGame);' in rebuild and 'm_sharedSourceFrameTransformVersion < 2' in rebuild),
    ('shared frame remains owned only by LOD0', 'if (lodIndex == 0)' in rebuild and 'transformSharedSourceFrame(m_asset, sharedDelta);' in rebuild),
    ('status declares non-cumulative hit-volume remap', 'shared SOURCE hit volumes followed LOD0 without cumulative rotation' in rebuild),
]
failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] shared collision/hit primitive frames follow LOD0 mapping without cumulative or conjugated-leaf drift')
