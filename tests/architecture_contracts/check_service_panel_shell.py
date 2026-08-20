#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def fail(msg):
    print(f'[FAIL] native F5-F8 service presentation: {msg}', file=sys.stderr)
    raise SystemExit(1)

router = read('src/ui/presentation/PresentationFunctionKeyRouter.cpp')
coord_h = read('src/ui/presentation/GamePresentationCoordinator.h')
coord = read('src/ui/presentation/GamePresentationCoordinator.cpp')
app = read('src/core/Application.cpp')
renderer = read('src/ui/presentation/InSessionPresentationRenderer.cpp')
service_h = read('src/ui/services/ServiceUiDefinition.h')

expected = (
    (5, 'GovernmentServices', 'service.government.title'),
    (6, 'Shipyard', 'service.shipyard.title'),
    (7, 'RepairRefit', 'service.repair_refit.title'),
    (8, 'Trade', 'service.trade.title'),
)
service_sources = ''.join(read(f'src/ui/services/{name}') for name in (
    'GovernmentServicesUi.cpp','ShipyardUi.cpp','RepairRefitUi.cpp','TradeUi.cpp'))
for key, enum_name, title_key in expected:
    if f'ServiceUiId::{enum_name}' not in service_sources:
        fail(f'F{key} service definition missing')
    if f'        {key},' not in service_sources:
        fail(f'F{key} service function-key registry entry missing')
    if title_key not in service_sources:
        fail(f'localized native title key missing: {title_key}')
if 'findServiceUiDefinitionByFunctionKey(functionKey)' not in router:
    fail('F5-F8 router no longer delegates to the service registry')

if 'mode == GameUiMode::ServicePanel;' in coord_h[coord_h.find('isFullScreenWebDocument'):coord_h.find('};', coord_h.find('isFullScreenWebDocument'))]:
    fail('ServicePanel is again classified as a WebView document')
if 'target.mode != GameUiMode::ServicePanel' not in coord:
    fail('ServicePanel is not a scene-backed coordinator target')
if 'm_gameUi.armSceneTarget(target);' not in app[app.find('if (target.mode == GameUiMode::ServicePanel)'):]:
    fail('Application does not arm native F5-F8 scene target')
for forbidden in ('service_shell.html', 'service_panel.js', 'service_panel_ready|', 'service_panel_prepared|'):
    if forbidden in app + service_h:
        fail(f'legacy browser service seam survived in production code: {forbidden}')
    if (ROOT / 'src/assets/webui' / forbidden).exists():
        fail(f'legacy browser service asset survived: {forbidden}')
for token in ('renderServicePanel', 'solidRectPx', 'definition->titleKey', 'definition->englishTitle'):
    if token not in renderer:
        fail(f'native service renderer contract missing: {token}')

print('[PASS] F5-F8 are native scene-backed peer presentation targets')
