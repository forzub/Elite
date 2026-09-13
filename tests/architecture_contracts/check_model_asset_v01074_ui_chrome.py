#!/usr/bin/env python3
"""v0.10.74: shared high-tech UI chrome + contextual help without reopening stage cores."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
VERSION = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
HTML = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')
MODEL_REL = 'src/assets/webui/model_asset_editor/ui/chrome_model.js'
EFFECT_REL = 'src/assets/webui/model_asset_editor/effects/ui_chrome_base.js'
COMPOSITION_REL = 'src/assets/webui/model_asset_editor/effects/ui_chrome.js'
I18N_REL = 'src/assets/webui/model_asset_editor/effects/i18n.js'
MODEL = (ROOT / MODEL_REL).read_text(encoding='utf-8')
EFFECT = (ROOT / EFFECT_REL).read_text(encoding='utf-8')
COMPOSITION = (ROOT / COMPOSITION_REL).read_text(encoding='utf-8')
I18N = (ROOT / I18N_REL).read_text(encoding='utf-8')

if 'ModelAssetEditorVersion = "0.10.75"' not in VERSION:
    raise AssertionError('editor version is not 0.10.75')

for path in (ROOT / MODEL_REL, ROOT / EFFECT_REL, ROOT / COMPOSITION_REL):
    if not path.is_file():
        raise AssertionError(f'missing UI chrome module: {path.relative_to(ROOT)}')

if "import './ui_chrome.js';" not in I18N:
    raise AssertionError('I18N effect must load the shared UI chrome composition effect')
if I18N.count("import './ui_chrome.js';") != 1:
    raise AssertionError('UI chrome bootstrap import must occur exactly once')
if "import './ui_chrome_base.js';" not in COMPOSITION:
    raise AssertionError('v0.10.75 UI composition must preserve the accepted v0.10.74 chrome adapter')

# UI chrome is a new subsystem born physically separated. It must not be copied
# into the composition HTML or any frozen stage-core/presentation module.
if 'uiChromeHelpButton' in HTML or 'uiChromeHelpModal' in HTML:
    raise AssertionError('UI chrome implementation leaked into model_asset_editor.html')
for rel in (
    'src/assets/webui/model_asset_editor/core/source.js',
    'src/assets/webui/model_asset_editor/core/lods.js',
    'src/assets/webui/model_asset_editor/core/geometry.js',
    'src/assets/webui/model_asset_editor/core/surfaces.js',
    'src/assets/webui/model_asset_editor/semantics/workspace.js',
    'src/assets/webui/model_asset_editor/physics/stage.js',
    'src/assets/webui/model_asset_editor/damage/stage.js',
    'src/assets/webui/model_asset_editor/final_assembly/validation.js',
    'src/assets/webui/model_asset_editor/final_assembly/build.js',
):
    source=(ROOT / rel).read_text(encoding='utf-8')
    if 'uiChrome' in source or 'chrome_model' in source or 'ui_chrome' in source:
        raise AssertionError(f'UI chrome leaked into frozen stage module: {rel}')

# New modules deliberately expose anonymous object APIs so the established
# 546 named-function ownership census is not re-opened by visual chrome work.
for rel,source in ((MODEL_REL,MODEL),(EFFECT_REL,EFFECT),(COMPOSITION_REL,COMPOSITION)):
    if re.search(r'\bfunction\s+[A-Za-z_$][\w$]*\s*\(', source):
        raise AssertionError(f'new UI module introduced owned named functions without an ownership wave: {rel}')

for token in (
    'normalizeText:', 'keepVisible:', 'topicFor:',
    "'stage:source'", "'stage:lods'", "'stage:geometry'", "'stage:surfaces'", "'stage:semantics'",
    "'stage:physics'", "'stage:damage'", "'stage:validate'", "'stage:build'",
):
    if token not in MODEL:
        raise AssertionError(f'UI chrome pure model missing {token!r}')

for token in (
    "import chromeModel from '../ui/chrome_model.js';",
    'install=', 'MutationObserver', '.uiChromeHelpButton', '#uiChromeHelpModal',
    '.wizardStage', 'clip-path:', '.uiChromeHelpSource{display:none!important}',
    'panelSelector=', 'decorateWizard=', 'decorateStaticSections=',
    "#side .scroll > .section", "#wizardPanel",
):
    if token not in EFFECT:
        raise AssertionError(f'UI chrome effect missing {token!r}')

# The old permanent instruction wall remains source-compatible but is hidden by
# the chrome effect and surfaced through contextual help instead.
for token in ("q('#mainHelp')", "mainHelp.classList.add('uiChromeHelpSource')", "helpEntries.set('section:editor'"):
    if token not in EFFECT:
        raise AssertionError(f'global help migration missing {token!r}')

# Existing deployment globs are recursive; verify the source-side convention the
# physical-module gate depends on remains present.
CMAKE=(ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
for token in ('webui/model_asset_editor/*.js','--include model_asset_editor/*.js'):
    if token not in CMAKE:
        raise AssertionError(f'UI module deployment glob missing {token!r}')

print('[PASS] Model Asset Editor v0.10.74 UI chrome preserved under v0.10.75: reusable high-tech chrome/context help remains physically separated and stage cores remain untouched')
