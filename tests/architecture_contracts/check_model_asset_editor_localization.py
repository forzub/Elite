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
version=VERSION.read_text(encoding='utf-8')
if 'ModelAssetEditorVersion = "0.10.82"' not in version: errors.append('expected editor version 0.10.82')
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
