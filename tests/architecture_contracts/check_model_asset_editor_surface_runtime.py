from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
HTML=(ROOT/'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')
HANDLERS=(ROOT/'src/assets/webui/model_asset_editor/session/handlers.js').read_text(encoding='utf-8')
VERSION=(ROOT/'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
errors=[]
if 'surfaceGeometryVisible(' in HTML: errors.append('undefined legacy surfaceGeometryVisible call remains')
expected="surfaceDisplayRows.filter(r=>surfaceGeometryVisibility(effectiveGeometry(r,lod)).checked).length"
if expected not in HTML: errors.append('SURFACES visible-count path does not use authoritative visibility adapter')
if 'state.surfaceAnalysisRequested=false;' not in HANDLERS or 'state.surfaceAnalysisReady=true;' not in HANDLERS:
 errors.append('surface-analysis result does not close requested->ready transition')
if 'ModelAssetEditorVersion = "0.10.85"' not in VERSION: errors.append('expected editor version 0.10.85')
if errors:
 print('MODEL ASSET EDITOR SURFACE RUNTIME: FAIL')
 for error in errors: print(' -',error)
 sys.exit(1)
print('MODEL ASSET EDITOR SURFACE RUNTIME: PASS')
print(' - SURFACES render path uses the authoritative visibility function')
print(' - analysis result closes requested -> ready without undefined UI calls')
