#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "src/assets/webui/model_asset_editor"
HTML = ROOT / "src/assets/webui/model_asset_editor.html"

EXPECTED = [
    WEB / "app/actions.js",
    WEB / "app/workflow.js",
    WEB / "app/reducer.js",
    WEB / "app/store.js",
    WEB / "app/controller.js",
    WEB / "app/selectors.js",
    WEB / "app/state.js",
    WEB / "app/bootstrap.js",
    WEB / "APP_STATE_ARCHITECTURE.md",
]


def fail(message: str) -> None:
    print(f"MODEL ASSET EDITOR APPLICATION STATE: FAIL\n - {message}")
    sys.exit(1)


for path in EXPECTED:
    if not path.is_file():
        fail(f"missing {path.relative_to(ROOT)}")

read = lambda p: p.read_text(encoding="utf-8")
actions = read(WEB / "app/actions.js")
workflow = read(WEB / "app/workflow.js")
reducer = read(WEB / "app/reducer.js")
store = read(WEB / "app/store.js")
controller = read(WEB / "app/controller.js")
selectors = read(WEB / "app/selectors.js")
state = read(WEB / "app/state.js")
bootstrap = read(WEB / "app/bootstrap.js")
i18n = read(WEB / "effects/i18n.js")
html = read(HTML)

required_stages = ["source", "lods", "geometry", "surfaces", "semantics", "physics", "damage", "validate", "build"]
for stage in required_stages:
    if not re.search(rf"['\"]{re.escape(stage)}['\"]", workflow):
        fail(f"workflow is missing stage {stage!r}")

if "WORKFLOW_STAGE_REQUESTED" not in actions or "CONTROL_FIELD_SET" not in actions:
    fail("actions do not define explicit workflow/control actions")
if "reduceWorkflow" not in workflow or "applicationReducer" not in reducer:
    fail("workflow/application reducer boundary is missing")
if "createApplicationStore" not in store or "subscribe" not in store:
    fail("application store is missing dispatch/subscription semantics")
if "createApplicationController" not in controller or "requestStage" not in controller:
    fail("application controller does not own stage requests")
if "selectWorkflowStage" not in selectors or "selectApplicationSnapshot" not in selectors:
    fail("application selectors are missing")
if "Object.defineProperty(legacyState,'wizardStage'" not in state:
    fail("legacy wizardStage is not projected from authoritative application state")
if "applicationController" not in state or "applicationStore" not in state or "applicationState" not in state:
    fail("application state bridge is not exposed on the editor state object")
if "installApplicationState" not in bootstrap or "./state.js" not in bootstrap:
    fail("dedicated application bootstrap does not install the state layer")
if "installApplicationState" in i18n or "../app/state.js" in i18n:
    fail("i18n effect still owns application-state bootstrap")
if "bootstrapModelAssetEditorApplication(state)" not in html:
    fail("HTML shell does not invoke the dedicated application bootstrap")
if "class EditorViewState" not in html or "editorView:editorViewState" not in html:
    fail("EditorViewState must remain the authoritative viewport/view-state owner during this pass")

# Reducers/controllers must stay free of browser, rendering, transport and filesystem effects.
for name, text in [("workflow", workflow), ("reducer", reducer), ("store", store), ("controller", controller)]:
    for forbidden in ["document.", "window.", "THREE.", "WebSocket", "fetch(", "send(", "requestAnimationFrame", "localStorage"]:
        if forbidden in text:
            fail(f"{name} layer contains forbidden effect dependency {forbidden!r}")

# The application reducer must not absorb authored asset/domain payloads or THREE runtime objects.
for forbidden in ["renderLods", "semanticNodes", "geometries", "collisionGroup", "socketGroup", "new THREE"]:
    if forbidden in reducer:
        fail(f"application reducer is taking ownership of domain/runtime data via {forbidden!r}")

# Main HTML may still contain the legacy transition/effect adapter, but there must be no goto-style navigation.
if re.search(r"\bgoto\b", html, flags=re.IGNORECASE):
    fail("goto-style application navigation found in editor shell")

print("MODEL ASSET EDITOR APPLICATION STATE: PASS")
print(" - canonical workflow is reducer/store/controller driven")
print(" - PHYSICS is part of the asset-authoring workflow")
print(" - legacy state writes project into authoritative application state")
print(" - EditorViewState remains the viewport/view-state authority")
print(" - reducer/controller layers are free of DOM/THREE/RPC effects")
