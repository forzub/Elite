#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "src/assets/webui/model_asset_editor"
HTML = ROOT / "src/assets/webui/model_asset_editor.html"


def fail(message: str) -> None:
    print(f"MODEL ASSET EDITOR ORCHESTRATION LAYERS: FAIL\n - {message}")
    sys.exit(1)


read = lambda p: p.read_text(encoding="utf-8")
required = [WEB / "app/bootstrap.js", WEB / "app/stage_renderers.js", WEB / "effects/workflow.js"]
for path in required:
    if not path.is_file():
        fail(f"missing {path.relative_to(ROOT)}")

html = read(HTML)
bootstrap = read(WEB / "app/bootstrap.js")
renderers = read(WEB / "app/stage_renderers.js")
workflow_effects = read(WEB / "effects/workflow.js")
i18n = read(WEB / "effects/i18n.js")

if "bootstrapModelAssetEditorApplication" not in html:
    fail("HTML shell does not use the dedicated application bootstrap")
if "installApplicationState" in i18n:
    fail("i18n still installs application state")
if "createWorkflowEffects" not in html or "const {setWizardStage}=createWorkflowEffects" not in html:
    fail("stage transition effects are still owned directly by the HTML shell")
if re.search(r"function\s+setWizardStage\s*\(", html):
    fail("imperative setWizardStage implementation remains in HTML")
if "createWizardStageRendererRegistry" not in html or "renderWizardStage(" not in html:
    fail("HTML shell does not use the declarative stage renderer registry")

match = re.search(r"function\s+renderWizardPanelContents\s*\(\)\s*\{(?P<body>.*?)\n\}\nfunction\s+captureEditorViewTransition", html, flags=re.DOTALL)
if not match:
    fail("cannot locate renderWizardPanelContents boundary")
panel_body = match.group("body")
if re.search(r"\bif\s*\(\s*stage\s*===", panel_body):
    fail("stage if-dispatch remains in renderWizardPanelContents")
if "renderWizardStage(" not in panel_body:
    fail("renderWizardPanelContents does not delegate through the registry")

for stage in ["source", "lods", "geometry", "surfaces", "semantics", "physics", "damage", "validate", "build"]:
    if not re.search(rf"\b{stage}\s*:", renderers):
        fail(f"stage renderer registry is missing {stage}")

for forbidden in ["document.", "window.", "THREE.", "WebSocket", "fetch(", "send("]:
    if forbidden in renderers:
        fail(f"pure stage renderer registry contains forbidden effect dependency {forbidden!r}")

if "state.wizardStage=id" not in workflow_effects:
    fail("workflow effect does not request the authoritative stage transition")
if "stageRefresh" not in workflow_effects or "lodDetailStages" not in workflow_effects:
    fail("workflow redraw fan-out is not declarative")
if "assertEditorViewTransitionPreserved" not in workflow_effects or "assertEditorViewInvariant" not in workflow_effects:
    fail("workflow effect dropped EditorViewState transition invariants")
if "installApplicationState" not in bootstrap:
    fail("dedicated bootstrap is not wired to application state installation")

print("MODEL ASSET EDITOR ORCHESTRATION LAYERS: PASS")
print(" - application bootstrap is independent from i18n")
print(" - stage transitions are isolated in an effect adapter")
print(" - stage rendering uses a declarative registry")
print(" - HTML no longer owns setWizardStage or stage if-dispatch")
print(" - EditorViewState transition invariants remain enforced")
