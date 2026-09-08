#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
session = (ROOT / 'tools/model_asset_editor/ModelAssetEditorSession.cpp').read_text(encoding='utf-8')
html = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')

def body_between(text, begin, end):
    a = text.index(begin)
    b = text.index(end, a)
    return text[a:b]

select = body_between(
    session,
    'bool ModelAssetEditorSession::selectAsset(const std::string& id, bool forceReimport)',
    'bool ModelAssetEditorSession::saveWorkingAsset(bool quiet)',
)

required = (
    'OPEN is an eager viewport-residency boundary',
    'Production adoption follows the same eager OPEN contract as',
    'if (!ensureAllLodsLoaded()) return false;',
    'sendAsset();',
)
for token in required:
    if token not in select:
        raise AssertionError(f'eager declared-LOD OPEN contract missing {token!r}')

if 'loadLodData(0, false' in select:
    raise AssertionError('WORKING OPEN still has a LOD0-only bootstrap load')
if 'ModelAssetBinary::loadLod(readPath.string(), m_asset, 0' in select:
    raise AssertionError('production OPEN still has a LOD0-only bootstrap load')
if 'sourceAuthority == CatalogSourceAuthority::Folder && !ensureAllLodsLoaded()' in select:
    raise AssertionError('eager OPEN is still conditional on folder SOURCE authority')

rebuild = body_between(
    session,
    'bool ModelAssetEditorSession::reimportLodSourcePartsInConfiguredBasis(',
    'bool ModelAssetEditorSession::reloadMeshFromSource(',
)
if 'ensureLodLoaded(lodIndex)' not in rebuild:
    raise AssertionError('per-LOD axis mapping lost explicit target-LOD residency guard')
if 'ensureAllLodsLoaded()' in rebuild:
    raise AssertionError('axis mapping started loading/mutating every LOD again')

if 'state.pendingActiveLod=li' not in html:
    raise AssertionError('explicit LOD switch no longer owns its pending viewport target')

print('[PASS] OPEN publishes every declared LOD while axis mapping remains target-LOD only')
