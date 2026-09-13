from pathlib import Path
import json,re,sys
ROOT=Path(__file__).resolve().parents[2]
CAT=ROOT/'src/assets/localization/ui/tools/model_asset_editor.json'
HTML=ROOT/'src/assets/webui/model_asset_editor.html'
WEB=ROOT/'src/assets/webui/model_asset_editor'
CPP=ROOT/'tools/model_asset_editor/ModelAssetEditorSession.cpp'
HDR=ROOT/'tools/model_asset_editor/ModelAssetEditorSession.h'
VERSION=ROOT/'tools/model_asset_editor/EditorVersion.h'
errors=[]
data=json.loads(CAT.read_text(encoding='utf-8')); locales=['en','ru','zh-Hans','es','ja']; strings=data.get('strings',{})
if data.get('locale_order')!=locales: errors.append(f'locale_order must be {locales!r}')
for key,row in strings.items():
 for locale in locales:
  if not str(row.get(locale,'')).strip(): errors.append(f'{key}: missing/empty {locale}')
# No full English prose copied into a non-English locale. Single technical tokens are allowed.
for key,row in strings.items():
 en=str(row.get('en','')).strip()
 if len(re.findall(r'[A-Za-z]{3,}',en))<3: continue
 for locale in locales[1:]:
  if str(row.get(locale,'')).strip()==en: errors.append(f'{key}: {locale} copies full English prose')
html=HTML.read_text(encoding='utf-8')
files=[HTML,*WEB.rglob('*.js')]
all_text='\n'.join(p.read_text(encoding='utf-8') for p in files)
refs=set(re.findall(r'model_editor\.[A-Za-z0-9_.-]+',all_text))
for key in sorted(refs):
 if key.endswith('.'): continue
 if key not in strings: errors.append(f'referenced localization key missing: {key}')
for key in re.findall(r'data-i18n(?:-title|-aria|-placeholder)?="([^"]+)"',html):
 if key not in strings: errors.append(f'HTML localization key missing: {key}')
if 'data-i18n="model_editor.status.language_shortcut"' not in html: errors.append('status bar language shortcut is not declaratively localized')
i18n=(WEB/'effects/i18n.js').read_text(encoding='utf-8')
if 'acceptSettingsLocale' not in i18n or 'pendingLocalePersist' not in i18n: errors.append('early locale override/persistence arbitration missing')
settings=(WEB/'persistence/settings.js').read_text(encoding='utf-8')
if 'try{acceptSettingsLocale(locale);}' not in settings or "refreshLocale(msg.locale,'settings')" not in settings: errors.append('settings arrival can overwrite early locale choice')
chrome=(WEB/'ui/chrome_model.js').read_text(encoding='utf-8')
workflow=(WEB/'ui/semantics_workflow_model.js').read_text(encoding='utf-8')
for name,text in [('chrome_model',chrome),('semantics_workflow_model',workflow)]:
 if "lang==='ru'" in text or 'lang === \'ru\'' in text or "locale='en'" in text: errors.append(f'{name}: locale-specific RU/EN branch remains')
handlers=(WEB/'session/handlers.js').read_text(encoding='utf-8')
if 'msg.messageKey?tr(msg.messageKey' not in handlers: errors.append('client does not prefer structured backend messageKey')
cpp=CPP.read_text(encoding='utf-8'); hdr=HDR.read_text(encoding='utf-8')
if 'messageKey' not in hdr or 'messageParams' not in hdr or 'payload["messageKey"]' not in cpp: errors.append('backend structured status localization protocol missing')
# High-value effect paths must not regress to direct user-visible status literals.
for rel in ['effects/lod_runtime.js','effects/semantics.js','viewport/attachments.js']:
 text=(WEB/rel).read_text(encoding='utf-8')
 for pat in [r"localStatus\(\s*'[^']*[A-Za-z]{3,}",r'localStatus\(\s*`[^`]*[A-Za-z]{3,}']:
  if re.search(pat,text): errors.append(f'{rel}: direct user-facing localStatus literal remains')

# High-visibility shell coverage. Technical identifiers (LOD0, XYZ, file names,
# numeric badges) may remain literal; human prose must be keyed or call tr().
for required in [
 'data-i18n="model_editor.busy.reading"',
 'data-i18n="model_editor.common.state"',
 'data-i18n="model_editor.radial.center_pivot"',
 'data-i18n="model_editor.radial.center_origin"',
 'data-i18n="model_editor.radial.center_custom"',
 'data-i18n="model_editor.axis_rotation.apply_button"',
 'data-i18n="model_editor.status.idle"',
 'data-i18n="model_editor.status.ready"'
]:
 if required not in html: errors.append(f'static shell localization marker missing: {required}')
for forbidden in [
 '>No used default geometry in this LOD.<',
 '>LOD not loaded<',
 '>Selected element pivot<',
 '>Parent origin (0,0,0)<',
 '>REBUILD LOD0 + APPLY ROTATION<',
 '>Ready<',
 '>IDLE<'
]:
 if forbidden in html and 'data-i18n' not in html[max(0,html.find(forbidden)-180):html.find(forbidden)+len(forbidden)+50]:
  errors.append(f'unkeyed static/user prose remains: {forbidden}')
for pattern,label in [
 (r"localStatus\(\s*`[^`]*mesh families selected",'surface selection status'),
 (r"confirm\(\s*`Delete \$\{unused\.length\}",'unused geometry confirmation'),
 (r"confirm\(\s*`Reload \$\{g\.id\} directly from SOURCE",'source reload confirmation'),
 (r"<span>source meshes</span>",'storage labels'),
 (r"GAME FRAME · FIXED</span>",'axis legend')
]:
 if re.search(pattern,html): errors.append(f'direct localization bypass remains: {label}')

# Acceptance regressions from v0.10.85: language must be selectable before any
# asset/settings payload, dynamic connection state must not be overwritten by
# static DOM localization, and locale changes must repaint the viewport legend.
if 'id="toolbarLanguage"' not in html: errors.append('always-available toolbar language selector missing')
if "$('toolbarLanguage').onchange=()=>persistLocale" not in html: errors.append('toolbar language selector is not immediate')
if 'persistLocale,cycleLocale' not in html: errors.append('persistLocale is not exposed to shell composition')
if 'id="status" data-i18n=' in html: errors.append('dynamic connection status is still declaratively reset by locale refresh')
if 'function refreshDynamicUi(){updateAxisLegend();' not in html: errors.append('locale refresh does not repaint axis legend')
if "tr('model_editor.version.asset'" not in handlers: errors.append('version badge asset label bypasses localization')
for required in ['model_editor.version.asset','model_editor.status.working_revision','model_editor.lod.kind.source','model_editor.lod.kind.generated']:
 if required not in strings: errors.append(f'acceptance localization key missing: {required}')
for key,locale,forbidden in [
 ('model_editor.v4.section.lods','zh-Hans',['Render']),
 ('model_editor.v4.lod.independent_note','zh-Hans',['manifest']),
 ('model_editor.lod_workspace.lod_help','zh-Hans',['PREPARE']),
 ('model_editor.v4.section.lods','es',['Render LOD']),
 ('model_editor.v4.lod.independent_note','es',[' manifest']),
 ('model_editor.lod_workspace.lod_help','ja',['PREPARE'])
]:
 value=str(strings.get(key,{}).get(locale,''))
 for token in forbidden:
  if token in value: errors.append(f'{key}: {locale} still contains visible English token {token!r}')

# Fallback English is permitted only when a locale value is actually absent.
# A present non-English locale may not silently copy full English prose.
version=VERSION.read_text(encoding='utf-8')
if 'ModelAssetEditorVersion = "0.10.85"' not in version: errors.append('expected editor version 0.10.85')
for required in [
 'model_editor.physical_scale.title','model_editor.surfaces.selection_help','model_editor.geometry_inventory.shared_tip',
 'model_editor.maintenance.scan_metrics','model_editor.overlay.render_detail','model_editor.common.detached'
]:
 if required not in strings: errors.append(f'visible localization key missing: {required}')
for forbidden in [
 'GAME LINK · NOT LINKED — с игрой пока связи нет.',
 'Shared geometry properties come from ${effectiveId}. SOURCE provenance remains',
 'SOURCE CURRENT · all stored file hashes match</div>',
 'render LOD${state.activeLod}: ${rn.id}\ngeometry:'
]:
 if forbidden in all_text: errors.append(f'visible English bypass remains: {forbidden}')
# Final high-visibility polish: raw shell/effect presentation must stay keyed.
for forbidden in [
 "needsPrepare:'NEEDS PREPARE'",
 "showAll:'ПОКАЗАТЬ ВСЕ',hideAll:'СПРЯТАТЬ ВСЕ'",
 'aria-label=\"allow replacement\"',
 'aria-label=\"preview replacement\"',
 "let geom='<option value=\"-1\">none</option>'",
 "let refs='<option value=\"-1\">select reference element…</option>'",
 "body.textContent=g.isInstanceAlias?'INST'",
 "bytes.textContent=g.isInstanceAlias?'LINK'",
 "label:active?'AUTO ORIENTATION'",
 "if(label)label.textContent=info?`SELECTED · LOD"
]:
 if forbidden in html or forbidden in all_text: errors.append(f'final localization bypass remains: {forbidden}')
if errors:
 print('MODEL ASSET EDITOR LOCALIZATION ARCHITECTURE: FAIL')
 for error in errors: print(' -',error)
 sys.exit(1)
print('MODEL ASSET EDITOR LOCALIZATION ARCHITECTURE: PASS')
print(' - one five-locale catalog is structurally complete')
print(' - static DOM localization is declarative')
print(' - locale switching works before asset/settings arrival and persists later')
print(' - contextual help and SEMANTICS workflow no longer contain RU-vs-EN forks')
print(' - backend status protocol supports stable localization keys')
print(' - selected high-visibility effect paths reject direct status literals')
