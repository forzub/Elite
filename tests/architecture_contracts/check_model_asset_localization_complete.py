#!/usr/bin/env python3
"""Model Asset Editor localization completeness contract."""
from pathlib import Path
import html as html_module
import json
import re

ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "src/assets/localization/ui/tools/model_asset_editor.json"
data = json.loads(PATH.read_text(encoding="utf-8"))

expected_locales = ["en", "ru", "zh-Hans", "es", "ja"]
locales = data.get("locale_order")
if locales != expected_locales:
    raise AssertionError(f"unexpected locale_order: {locales!r}")

strings = data.get("strings")
if not isinstance(strings, dict) or not strings:
    raise AssertionError("localization strings map is missing or empty")

missing = []
for key, row in strings.items():
    if not isinstance(row, dict):
        missing.append((key, "<row>"))
        continue
    for locale in expected_locales:
        if not str(row.get(locale, "")).strip():
            missing.append((key, locale))
if missing:
    sample = ", ".join(f"{key}:{locale}" for key, locale in missing[:20])
    raise AssertionError(f"missing translations ({len(missing)}): {sample}")

# Exact English copies are acceptable only for intentionally shared technical /
# international tokens or words whose spelling is genuinely the same.
allowed_same_as_english = {
    ("model_editor.toolbar.sockets.title", "es"),
    ("model_editor.action.socket", "es"),
    ("model_editor.status.error", "es"),
    ("model_editor.geometry_variants.original", "es"),
    ("model_editor.source.base", "es"),
    ("model_editor.preflight.class_auto", "es"),
    ("model_editor.preflight.raw_short", "ru"),
    ("model_editor.preflight.raw_short", "zh-Hans"),
    ("model_editor.preflight.raw_short", "es"),
    ("model_editor.preflight.raw_short", "ja"),
    ("model_editor.preflight.ok", "ru"),
    ("model_editor.preflight.ok", "es"),
    ("model_editor.preflight.ok", "ja"),
    ("model_editor.preflight.auto_count", "es"),
    ("model_editor.preflight.manual_count", "es"),
    ("model_editor.surfaces.material_editor", "es"),
    ("model_editor.surfaces.material", "es"),
    ("model_editor.maintenance.issue_lods", "es"),
    ("model_editor.geometry_compare.reference_short", "es"),
}
identical = []
for key, row in strings.items():
    en = str(row.get("en", "")).strip()
    if not en:
        continue
    for locale in expected_locales[1:]:
        if str(row.get(locale, "")).strip() == en and (key, locale) not in allowed_same_as_english:
            identical.append((key, locale, en))
if identical:
    sample = ", ".join(f"{key}:{locale}={value!r}" for key, locale, value in identical[:20])
    raise AssertionError(f"unexpected English fallback copies ({len(identical)}): {sample}")

# The accepted rotation dialog must use localized strings, not English-only UI text.
required_axis_keys = {
    "model_editor.axis_rotation.title",
    "model_editor.axis_rotation.frame_title",
    "model_editor.axis_rotation.frame_legend",
    "model_editor.axis_rotation.hint",
    "model_editor.axis_rotation.positive_hint",
    "model_editor.axis_rotation.reset_pending",
    "model_editor.axis_rotation.source_no_rotation",
    "model_editor.axis_rotation.cancel",
    "model_editor.axis_rotation.pending",
    "model_editor.axis_rotation.pending_none",
    "model_editor.axis_rotation.final_orientation",
    "model_editor.axis_rotation.rotation_only",
    "model_editor.axis_rotation.apply_scope",
    "model_editor.axis_rotation.apply_button",
}
missing_axis = sorted(required_axis_keys - strings.keys())
if missing_axis:
    raise AssertionError(f"axis rotation localization keys missing: {missing_axis}")



# Every literal tr('...') key used by the WebUI must resolve in the dictionary.
# A missing key would silently activate the fallback string and can therefore
# look "localized" in English while every other locale is wrong.
WEB = (ROOT / "src/assets/webui/model_asset_editor.html").read_text(encoding="utf-8")
used_tr_keys = set(re.findall(r"\btr\(\s*['\"]([^'\"]+)['\"]", WEB))
unknown_tr_keys = sorted(used_tr_keys - strings.keys())
if unknown_tr_keys:
    raise AssertionError(f"WebUI tr() calls reference missing localization keys: {unknown_tr_keys[:30]}")

# Hardcoded user-facing regression fence for controlled dynamic UI. SOURCE / LODS /
# GEOMETRY / SURFACES remain protected by their accepted byte/fingerprint contracts;
# this scanner intentionally targets the unfrozen shared + SEMANTICS/PHYSICS/DAMAGE
# surfaces so a future dynamic panel cannot bypass tr() again.
def balanced_function(src,name):
    marker=f'function {name}('
    start=src.find(marker)
    if start<0: raise AssertionError(f'controlled localization function missing: {name}')
    opening=src.find('{',start); depth=0; quote=None; esc=False; line=False; block=False; i=opening
    while i<len(src):
        c=src[i]; n=src[i+1] if i+1<len(src) else ''
        if line:
            if c=='\n': line=False
            i+=1; continue
        if block:
            if c=='*' and n=='/': block=False; i+=2; continue
            i+=1; continue
        if quote:
            if esc: esc=False
            elif c=='\\': esc=True
            elif c==quote: quote=None
            i+=1; continue
        if c=='/' and n=='/': line=True;i+=2;continue
        if c=='/' and n=='*': block=True;i+=2;continue
        if c in "'\"`": quote=c;i+=1;continue
        if c=='{': depth+=1
        elif c=='}':
            depth-=1
            if depth==0:return src[start:i+1]
        i+=1
    raise AssertionError(f'unbalanced function {name}')

def mask_calls(src,name='tr'):
    out=[];i=0;pat=name+'('
    while True:
        j=src.find(pat,i)
        if j<0:out.append(src[i:]);break
        if j>0 and (src[j-1].isalnum() or src[j-1] in '_$'):
            out.append(src[i:j+len(name)]);i=j+len(name);continue
        out.append(src[i:j]);k=j+len(name);depth=0;quote=None;esc=False;line=False;block=False
        while k<len(src):
            c=src[k];n=src[k+1] if k+1<len(src) else ''
            if line:
                if c=='\n':line=False
                k+=1;continue
            if block:
                if c=='*' and n=='/':block=False;k+=2;continue
                k+=1;continue
            if quote:
                if esc:esc=False
                elif c=='\\':esc=True
                elif c==quote:quote=None
                k+=1;continue
            if c=='/' and n=='/':line=True;k+=2;continue
            if c=='/' and n=='*':block=True;k+=2;continue
            if c in "'\"`":quote=c;k+=1;continue
            if c=='(':depth+=1
            elif c==')':
                depth-=1
                if depth==0:k+=1;break
            k+=1
        out.append('__TR__');i=k
    return ''.join(out)

def extract_literals(src):
    out=[];i=0
    while i<len(src):
        if src[i] not in "'\"`":i+=1;continue
        q=src[i];start=i;i+=1;buf=[];esc=False
        while i<len(src):
            c=src[i]
            if esc:buf.append(c);esc=False;i+=1;continue
            if c=='\\':buf.append(c);esc=True;i+=1;continue
            if c==q:out.append((q,''.join(buf),start));i+=1;break
            buf.append(c);i+=1
    return out

def strip_template_expr(text):
    out=[];i=0
    while i<len(text):
        if text.startswith('${',i):
            depth=1;i+=2;quote=None;esc=False
            while i<len(text) and depth:
                c=text[i]
                if quote:
                    if esc:esc=False
                    elif c=='\\':esc=True
                    elif c==quote:quote=None
                else:
                    if c in "'\"`":quote=c
                    elif c=='{':depth+=1
                    elif c=='}':depth-=1
                i+=1
            out.append('__EXPR__')
        else:out.append(text[i]);i+=1
    return ''.join(out)

TECH_WORDS={'RN','LOD','G','ID','RGB','FOV','X','Y','Z','T','M','N','NM','OBB','MIN','MAX'}
TECH_EXACT={'camera.cockpit / weapon.small / container.iso20'}
def residual_words(text):
    x=html_module.unescape(text).replace('__TR__',' ').replace('__EXPR__',' ')
    x=x.replace('\\n',' ')
    if x.strip() in TECH_EXACT:return []
    # dynamic data fragments such as RN12 / LOD3 / G7 are not prose
    words=re.findall(r'[A-Za-zА-Яа-яЁё一-龥ぁ-んァ-ヶ]{2,}',x)
    return [w for w in words if w.upper() not in TECH_WORDS]

def scan_html_literals(label,src):
    masked=mask_calls(src)
    issues=[]
    for _,literal,pos in extract_literals(masked):
        if '<' not in literal:continue
        rendered=strip_template_expr(literal)
        for m in re.finditer(r'>([^<>]+)<',rendered):
            words=residual_words(m.group(1))
            if words: issues.append((label,'text',m.group(1).strip(),words))
        for m in re.finditer(r'\b(?:title|placeholder|aria-label)="([^"]+)"',rendered):
            words=residual_words(m.group(1))
            if words:issues.append((label,'attribute',m.group(1).strip(),words))
    return issues

names=[]
for m in re.finditer(r'function\s+([A-Za-z0-9_$]+)\s*\(',WEB):
    n=m.group(1)
    if n.startswith(('semantic','structural')) or n in {
      'renderSharedStageMeshPanel','fitView','renderNodeInspector','renderRenderNodeInspector',
      'renderDamageSemantics','renderCollisionInspector','renderSocketInspector','openSettings','translateServerMessage'}:
      names.append(n)
issues=[]
for n in names: issues.extend(scan_html_literals(n,balanced_function(WEB,n)))
# semantic branch inside the mixed renderWizardPanel function
start=WEB.index("if(stage==='semantics')");end=WEB.index("if(stage==='physics')",start)
issues.extend(scan_html_literals('renderWizardPanel[semantics]',WEB[start:end]))

# User-facing JS sinks in current unfrozen/shared code. After masking tr(), any
# remaining literal words in the first argument are uncontrolled UI text.
def scan_sinks(label,src):
  masked=mask_calls(src); found=[]
  pat=re.compile(r'\b(localStatus|status|confirm|prompt|alert|notice)\(\s*([\'\"`])')
  for m in pat.finditer(masked):
    q=m.group(2);i=m.end();buf=[];esc=False
    while i<len(masked):
      c=masked[i]
      if esc:buf.append(c);esc=False;i+=1;continue
      if c=='\\':buf.append(c);esc=True;i+=1;continue
      if c==q:break
      buf.append(c);i+=1
    raw=strip_template_expr(''.join(buf)); words=residual_words(raw)
    if words:found.append((label,m.group(1),raw[:180],words))
  return found
for n in names:issues.extend(scan_sinks(n,balanced_function(WEB,n)))
# event-handler tail is outside named functions but contains settings/add actions.
tail=WEB[WEB.index('function connect()'):]
issues.extend(scan_sinks('global-event-handlers',tail))


if issues:
    sample = "; ".join(f"{scope}/{kind}: {text!r}" for scope, kind, text, *_ in issues[:20])
    raise AssertionError(f"hardcoded user-facing text escaped tr() in controlled dynamic UI ({len(issues)}): {sample}")

# The shared viewport selector was the concrete RU-fallback regression that exposed
# this class of bug; keep it explicitly wired into applyLocale().
for token in (
    'id="meshViewportModeSource"',
    'id="meshViewportModeDouble"',
    'id="meshViewportModeWorking"',
    "model_editor.viewport.mode.source",
    "model_editor.viewport.mode.double_sided",
    "model_editor.viewport.mode.working",
):
    if token not in WEB:
        raise AssertionError(f"localized viewport mode selector contract missing {token!r}")

print(f"[PASS] model asset editor localization complete + hardcoded dynamic UI fence: {len(strings)} keys × {len(expected_locales)} locales")
