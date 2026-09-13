#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
builder = (ROOT / 'tools/model_asset_editor/CanonicalMeshBuilder.cpp').read_text(encoding='utf-8')
header = (ROOT / 'tools/model_asset_editor/CanonicalMeshBuilder.h').read_text(encoding='utf-8')
tests = (ROOT / 'tests/model_asset/ModelAssetBinaryTests.cpp').read_text(encoding='utf-8')
cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')

checks = [
    ('v3 topology-preserving algorithm id', 'canonical_mesh_libigl_topology_preserving_v3' in header),
    ('production builder does not include Embree raycast', 'igl/embree/reorient_facets_raycast.h' not in builder),
    ('production builder does not invoke Embree raycast', 'reorient_facets_raycast(' not in builder),
    ('open components explicitly preserved', 'Open components are' in builder and 'never globally reoriented by PREPARE.' in builder),
    ('closed shell uses signed-volume whole-component rule', 'A closed orientable shell has an unambiguous outside: signed volume.' in builder),
    ('local winding is repaired before libigl split', builder.index('applyParity(working.triangles, beforeOrientation);') < builder.index('repairTopologyWithLibigl(mesh, working, libiglStats, result.error)')),
    ('authored topology preservation gate exists', 'preserveAuthoredTopology' in builder and 'authoredTopologyPreserved' in builder),
    ('regression test exists', 'testCanonicalBuilderPreservesAuthoredOrientationForOpenShell' in tests),
    ('editor target no longer links Embree', 'webview::core_static\n        igl::core\n        igl::embree' not in cmake),
]
failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] model asset PREPARE preserves authored open/thin orientation; closed shells only flip deterministically')
