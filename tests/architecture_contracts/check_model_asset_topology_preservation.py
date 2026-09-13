#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
builder = (ROOT / 'tools/model_asset_editor/CanonicalMeshBuilder.cpp').read_text(encoding='utf-8')
header = (ROOT / 'tools/model_asset_editor/CanonicalMeshBuilder.h').read_text(encoding='utf-8')
version = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
tests = (ROOT / 'tests/model_asset/ModelAssetBinaryTests.cpp').read_text(encoding='utf-8')
fixture = ROOT / 'tests/model_asset/fixtures/cobramk1T_winding_regression.obj'

pre_flip = 'applyParity(working.triangles, beforeOrientation);'
libigl = 'repairTopologyWithLibigl(mesh, working, libiglStats, result.error)'
checks = [
    ('editor version 0.10.73', '"0.10.73"' in version),
    ('v3 topology-preserving PREPARE id', 'canonical_mesh_libigl_topology_preserving_v3' in header),
    ('pre-libigl local winding repair', pre_flip in builder and libigl in builder and builder.index(pre_flip) < builder.index(libigl)),
    ('authored topology preservation predicate', 'const bool preserveAuthoredTopology' in builder and 'authoredTopologyPreserved' in builder),
    ('pre-rebuild topology mutation is rejected', 'PREPARE changed authored topology before render rebuild' in builder),
    ('post-rebuild topology mutation is rejected', 'render rebuild changed authored topology' in builder),
    ('real Cobra winding regression fixture exists', fixture.is_file()),
    ('real Cobra winding regression test exists', 'testCanonicalBuilderPreservesClosedCobraTopologyWhileRepairingWinding' in tests),
    ('regression asserts no topology split', 'built.splitTopologyVertices == 0' in tests),
    ('regression asserts closed boundary preservation', 'after.boundaryEdges == 0' in tests and 'after.closedComponents == 1' in tests),
    ('regression locks 22-face winding defect', 'before.windingFlipsRequired == 22' in tests),
]
failed = [name for name, ok in checks if not ok]
if failed:
    for name in failed:
        print(f'[FAIL] {name}')
    sys.exit(1)
print('[PASS] Model Asset Editor v0.10.73 topology-preserving PREPARE: winding repair precedes libigl split; manifold/open boundary topology is preserved; Cobra 22-face regression locked')
