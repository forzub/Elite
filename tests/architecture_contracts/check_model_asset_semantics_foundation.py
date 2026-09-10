from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEB = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')
CAP = (ROOT / 'tools/model_asset_editor/EDITOR_CAPABILITIES.json').read_text(encoding='utf-8')


def require(token: str, where: str = 'web') -> None:
    haystack = WEB if where == 'web' else CAP
    if token not in haystack:
        raise AssertionError(f'SEMANTICS foundation missing {token!r} in {where}')


def body_between(start: str, end: str) -> str:
    a = WEB.index(start)
    b = WEB.index(end, a)
    return WEB[a:b]


# One authoritative visual LOD drives the SEMANTICS scene/table/binding editor.
for token in (
    "function assertSemanticVisualLodInvariant(",
    "assertEditorViewInvariant(`semantics:${context}`)",
    "state.renderNodeGroups.length!==expected",
    "function switchSemanticLod(lodIndex)",
    "switchEditorLod(target,'semantics')",
    "restoreSemanticSelectionAfterLodSwitch(primary,selected)",
    "rebuildScene(true)",
    "applySemanticMotionPreview(true)",
    "assertSemanticVisualLodInvariant('switch')",
    'data-semantic-visual-lod="${model.activeLod}"',
    "tr('model_editor.semantics.binding.title'",
):
    require(token)

switch_body = body_between('function switchSemanticLod(', 'function semanticTreeOrderKey')
if "state.activeLod=0" in switch_body or "LOD0" in switch_body:
    raise AssertionError('SEMANTICS LOD switch reintroduced a hard-coded LOD0 path')

# Logical semantic selection survives visual LOD changes even when stable RenderNode
# ids differ between LOD documents.
for token in (
    'function restoreSemanticSelectionAfterLodSwitch(',
    'editorViewState.selectedSemanticNode=validPrimary',
    "findIndex(rn=>Number(rn?.semanticNodeIndex)===validPrimary)",
):
    require(token)

# MODEL ROOT is a permanent implicit identity root, not a serialized semantic Node.
for token in (
    'data-semantic-model-root',
    "tr('model_editor.semantics.tree.asset_header'",
    "tr('model_editor.semantics.tree.structure_summary'",
    "send('set_node_parents',{nodeIndices:moving,placement:'inside',parentIndex:-1})",
    "tr('model_editor.semantics.status.root_no_connection'",
):
    require(token)
for forbidden in (
    'create_semantic_asset_root',
    'asset must have exactly one semantic root; found',
):
    if forbidden in WEB:
        raise AssertionError(f'SEMANTICS MODEL ROOT must stay implicit, found obsolete {forbidden!r}')

# TREE explode is rooted in the model/asset origin. STRUCTURAL GRAPH may still
# choose its own explicit endpoint root.
root_body = body_between('function semanticGraphRootIndex(', 'function semanticGraphRootCenter')
if "semanticStructureMode==='graph'" not in root_body or 'return-1;' not in root_body:
    raise AssertionError('TREE explode no longer uses implicit MODEL ROOT / asset origin')

# A single visual mesh is explicitly reported as non-separable rather than
# pretending the semantic explode can split geometry that does not exist.
for token in (
    'function semanticVisualLodProfile(',
    "tr('model_editor.semantics.tree.separation_unavailable'",
    "tr('model_editor.semantics.tree.separation_monolithic'",
):
    require(token)


# Unbound visuals in a higher LOD must still separate as temporary visual clusters.
# This is preview-only and must not invent semantic ownership or fuzzy bindings.
for token in (
    'function semanticUnboundRenderClusterRoot(',
    'function semanticUnboundRenderClusterOffsets(',
    'const unboundOffsets=semanticUnboundRenderClusterOffsets({lod,amount,semanticNodes:state.asset?.nodes||[]',
    'owner>=0?(state.semanticGraphOffsets.get(owner)||new THREE.Vector3()):(unboundOffsets.get(cluster)||new THREE.Vector3())',
    "tr('model_editor.semantics.tree.unbound_clusters'",
):
    require(token)

explode_body = body_between('function applySemanticGraphExplode(', 'function applySemanticMotionPreview')
for forbidden in ('semanticNodeIndex=', "send('set_render_node_semantic'"):
    if forbidden in explode_body:
        raise AssertionError('SEMANTICS unbound explode preview must not mutate semantic ownership')

# Capability registry now protects this semantics-only foundation.
for token in (
    'implicit MODEL ROOT',
    'data-semantic-model-root',
    'assertSemanticVisualLodInvariant',
    'restoreSemanticSelectionAfterLodSwitch',
    'semanticUnboundRenderClusterRoot',
    'semanticUnboundRenderClusterOffsets',
):
    require(token, 'cap')

print('[PASS] Model Asset Editor SEMANTICS authoritative visual LOD + implicit MODEL ROOT foundation')
