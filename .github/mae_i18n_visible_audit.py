from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
web=ROOT/'src/assets/webui/model_asset_editor'
html=ROOT/'src/assets/webui/model_asset_editor.html'
files=[html,*sorted(web.rglob('*.js'))]

print('=== DIRECT USER-FACING CALLS / ASSIGNMENTS ===')
patterns={
 'localStatus':re.compile(r"localStatus\(\s*([`'\"])(.*?)\1",re.S),
 'confirm':re.compile(r"confirm\(\s*([`'\"])(.*?)\1",re.S),
 'prompt':re.compile(r"prompt\(\s*([`'\"])(.*?)\1",re.S),
 'notice':re.compile(r"notice\(\s*([`'\"])(.*?)\1",re.S),
 'textContent':re.compile(r"textContent\s*=\s*([`'\"])(.*?)\1",re.S),
 'innerHTML':re.compile(r"innerHTML\s*=\s*([`'\"])(.*?)\1",re.S),
 'title':re.compile(r"\.title\s*=\s*([`'\"])(.*?)\1",re.S),
 'aria':re.compile(r"setAttribute\(\s*['\"]aria-label['\"]\s*,\s*([`'\"])(.*?)\1",re.S),
}
count=0
for p in files:
    text=p.read_text(encoding='utf-8')
    for label,pat in patterns.items():
        for m in pat.finditer(text):
            value=m.group(2).replace('\n',' ').strip()
            if not re.search(r'[A-Za-zА-Яа-яЁё]{3,}',value): continue
            # Already keyed/composed localization is fine.
            if 'tr(' in value or 'data-i18n' in value: continue
            line=text.count('\n',0,m.start())+1
            print(f'{p.relative_to(ROOT)}:{line} [{label}] {value[:220]}')
            count+=1
print('DIRECT_CANDIDATES',count)

print('\n=== STATIC HTML TEXT WITHOUT data-i18n ON SAME ELEMENT ===')
source=html.read_text(encoding='utf-8').split('<script type="module">',1)[0]
static_count=0
for m in re.finditer(r'<([A-Za-z][^<>]*?)>([^<>]+)</[^>]+>',source,re.S):
    opening=m.group(1); value=re.sub(r'\s+',' ',m.group(2)).strip()
    if not value or not re.search(r'[A-Za-z]{3,}',value): continue
    if 'data-i18n' in opening: continue
    if re.fullmatch(r'(?:LOD\d*|G#|RN|X|Y|Z|XYZ|[+\-]?\d+°|×|—|\?|SAVE —|\+\s*[A-Z]+)',value): continue
    line=source.count('\n',0,m.start())+1
    print(f'{html.relative_to(ROOT)}:{line} [static] {value[:220]}')
    static_count+=1
print('STATIC_CANDIDATES',static_count)

print('\n=== TEMPLATE-LITERAL ENGLISH FRAGMENTS (diagnostic; broad) ===')
template_count=0
for p in files:
    text=p.read_text(encoding='utf-8')
    for m in re.finditer(r'`([^`]{1,600})`',text,re.S):
        value=m.group(1)
        if 'tr(' in value or '<style' in value or 'linear-gradient' in value: continue
        words=re.findall(r'\b[A-Za-z]{4,}\b',value)
        if len(words)<2: continue
        # skip obvious technical/path/diagnostic-only templates
        if re.fullmatch(r'[A-Za-z0-9_:.${}/\\\-\s]+',value) and len(words)<4: continue
        line=text.count('\n',0,m.start())+1
        print(f'{p.relative_to(ROOT)}:{line} [template] {re.sub(chr(10)," ",value)[:240]}')
        template_count+=1
        if template_count>=250: break
    if template_count>=250: break
print('TEMPLATE_CANDIDATES_SHOWN',template_count)
