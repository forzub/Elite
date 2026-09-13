#!/usr/bin/env python3
"""v0.10.73 regression: LOD file controls must not depend on a missing global lodIcon helper."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
web = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')
version = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')

if 'ModelAssetEditorVersion = "0.10.73"' not in version:
    raise AssertionError('editor version is not 0.10.73')

m = re.search(r'function\s+renderLodFiles\s*\(\s*\)\s*\{(?P<body>.*?)\n\}', web, re.S)
if not m:
    raise AssertionError('renderLodFiles() not found')
body = m.group('body')

if re.search(r'(?<![\w$])lodIcon\s*\(', body):
    raise AssertionError('renderLodFiles still calls undeclared global lodIcon()')

required = [
    "const makeLodIcon=(glyph,titleKey,descKey)=>",
    "document.createElement('button')",
    "button.className='lodIcon'",
    "attachDynamicToolTip(button,titleKey,descKey)",
    "makeLodIcon('◉'",
    "makeLodIcon('↻'",
    "makeLodIcon('⏏'",
]
for needle in required:
    if needle not in body:
        raise AssertionError(f'missing v0.10.73 LOD-icon runtime contract: {needle}')

# The fix must stay local to the effectful renderer instead of adding a new
# top-level named function / hidden module dependency.
if re.search(r'function\s+lodIcon\s*\(', web):
    raise AssertionError('lodIcon must not return as a new global named helper')

print('[PASS] Model Asset Editor v0.10.73 LOD file controls: local icon factory + tooltip wiring; no missing global lodIcon dependency')
