#!/usr/bin/env python3
"""v0.10.73 regression contract: Cobra OPEN + SOURCE identity/relink.

Locks the two runtime defects found with canonical cobra_mk1 WORKING r31:
1. physically extracted axis_mapping.js must own the axis token table needed by
   saved custom `axis:+Z,+Y,-X` presets, so acceptAssetState() cannot abort and
   leave the previous scene visible;
2. an authored SOURCE folder is geometry authority independently of optional
   RuntimeAssembly semantic/bootstrap context. Catalog UI reports actual folder
   availability, and SOURCE can be explicitly relinked when the folder moved.
"""
from pathlib import Path

from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def body_between(body: str, start: str, end: str) -> str:
    a = body.index(start)
    b = body.index(end, a + len(start))
    return body[a:b]


version = text("tools/model_asset_editor/EditorVersion.h")
if 'ModelAssetEditorVersion = "0.10.73"' not in version:
    raise AssertionError("v0.10.73 SOURCE identity repair must bump the visible editor version")

axis = text("src/assets/webui/model_asset_editor/core/axis_mapping.js")
axis_tokens = "const axisDirectionTokens=['+X','-X','+Y','-Y','+Z','-Z'];"
if axis_tokens not in axis:
    raise AssertionError("axis_mapping.js does not own axisDirectionTokens")
if axis.index(axis_tokens) > axis.index("function sourceBasisMappingFromPreset"):
    raise AssertionError("axis token table must be module-local before custom preset parsing")
if "axisDirectionTokens.includes(x)" not in axis:
    raise AssertionError("custom axis:* preset validation no longer uses the module-local token table")

session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
header = text("tools/model_asset_editor/ModelAssetEditorSession.h")
web = load_source_bundle(ROOT)

catalog = body_between(
    session,
    "ModelAssetEditorSession::ModelAssetEditorSession(",
    "std::filesystem::path ModelAssetEditorSession::compiledPath",
)
for token in (
    "runtimeCobraFolderAvailable",
    "sourceFolderAssetAvailable(m_sourceAssetsRoot, runtimeCobraDirectory)",
    '"cobra_mk1"',
    "CatalogSourceAuthority::Folder",
    "CatalogBootstrapMode::RuntimeAssembly",
):
    if token not in catalog:
        raise AssertionError(f"canonical Cobra Folder/bootstrap split missing {token!r}")

send_catalog = body_between(
    session,
    "void ModelAssetEditorSession::sendCatalog()",
    "bool ModelAssetEditorSession::selectAsset(const std::string& id, bool forceReimport)",
)
for token in (
    "effectiveSourceDirectory",
    "sourceFolderAssetAvailable(m_sourceAssetsRoot, effectiveSourceDirectory)",
    '{"sourceAvailable", sourceAvailable}',
    '{"sourceDirectory", effectiveSourceDirectory.generic_string()}',
):
    if token not in send_catalog:
        raise AssertionError(f"catalog SOURCE-presence payload missing {token!r}")

if "sourceIcon=i.sourceAvailable?'📁':'📄'" not in web:
    raise AssertionError("asset selector must show folder/document icon from actual SOURCE availability")
render_catalog = body_between(web, "function renderCatalog()", "const commandLabels=")
if "[SOURCE]" in render_catalog or "[RUNTIME]" in render_catalog:
    raise AssertionError("asset selector must not expose SOURCE/RUNTIME authority labels")

if "bool setSourceAssetDirectory(const std::string& rawPath);" not in header:
    raise AssertionError("SOURCE relink backend API declaration is missing")
relink = body_between(
    session,
    "bool ModelAssetEditorSession::setSourceAssetDirectory(const std::string& rawPath)",
    "ModelAssetEditorSession::EditorAuthoringState ModelAssetEditorSession::captureEditorAuthoringState() const",
)
for token in (
    "resolveSourceFolderAssetRoot(m_sourceAssetsRoot, requested)",
    "sourceFolderAssetAvailable(m_sourceAssetsRoot, requested)",
    "catalog->sourceAuthority = CatalogSourceAuthority::Folder;",
    'invalidateWizardFrom("source");',
    '"source_directory_updated"',
    "sendCatalog();",
):
    if token not in relink:
        raise AssertionError(f"SOURCE relink lifecycle missing {token!r}")

handle = session[session.index("void ModelAssetEditorSession::handleMessage(const std::string& payload)"): ]
if 'command == "set_source_asset_directory"' not in handle:
    raise AssertionError("backend command router does not expose SOURCE relink")
if 'setSourceAssetDirectory(message.value("path", std::string()))' not in handle:
    raise AssertionError("SOURCE relink command does not forward the selected path")

if "data-maintenance-relink-source" not in web:
    raise AssertionError("SOURCE tab has no relink-folder control")
if "send('set_source_asset_directory',{path})" not in web:
    raise AssertionError("SOURCE relink control does not dispatch the backend command")
if "msg.type==='source_directory_updated'" not in web:
    raise AssertionError("frontend does not consume SOURCE relink result")

scan = body_between(
    session,
    "void ModelAssetEditorSession::sendSourceChangeScan()",
    "bool ModelAssetEditorSession::confirmSourceMeshDeletion(",
)
if "catalog->sourceAuthority != CatalogSourceAuthority::Folder" in scan:
    raise AssertionError("SOURCE scan is still gated by catalog enum instead of the linked folder")
for token in (
    "m_loadedSourceAssetDirectory.empty()",
    "sourceFolderAssetAvailable(m_sourceAssetsRoot, sourceDirectory)",
    "linked SOURCE folder is unavailable; choose a new SOURCE folder",
):
    if token not in scan:
        raise AssertionError(f"SOURCE scan/relink authority missing {token!r}")

print(
    "[PASS] Model Asset Editor v0.10.73 SOURCE identity repair: custom axis preset is module-closed; "
    "canonical Cobra prefers real Folder SOURCE with RuntimeAssembly bootstrap; selector folder/document icons "
    "and explicit SOURCE relink are wired"
)
