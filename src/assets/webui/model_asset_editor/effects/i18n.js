import uiChrome from './ui_chrome.js';
import {baseLocale,translateCatalog} from '../i18n/catalog.js';
import {applyLocalizedDom} from '../i18n/dom.js';
// Model Asset Editor effect adapter. Physical extraction wave7E: i18n / localized UI runtime.
const createI18nEffects=({state,$,refreshDynamicUi,localStatus,send,document,window,requestAnimationFrame,fetch,console})=>{
let userLocaleOverride=false,pendingLocalePersist=false;
function tr(key,fallback=key,vars={}){return translateCatalog(state.i18n,state.locale,key,fallback,vars);}
function applyDocumentTranslations(root=document){applyLocalizedDom(root,tr);}
function translateServerMessage(message){
 const text=String(message||'');
 if(text==='Running model topology / normals preflight...'||text==='Analyzing meshes for runtime-equivalent canonical preparation...'||text==='Analyzing meshes for canonical geometry preparation...'||text==='Checking canonical meshes and geometry contracts...')return tr('model_editor.server.preflight_running',text);
 if(text==='Importing source OBJ/assembly...')return tr('model_editor.server.source_importing',text);
 if(text==='Source assembly imported into independent render LODs')return tr('model_editor.server.source_loaded',text);
 if(text==='Asset loaded; semantic state is shared, render LOD graphs are independent')return tr('model_editor.server.asset_loaded',text);
 let m=text.match(/^Model preflight: (LOD0 ready|review required), invalid=(\d+), review=(\d+)$/);
 if(m)return `${tr('model_editor.preflight.title','КОНТРАКТ ГЕОМЕТРИИ')}: ${m[1]==='LOD0 ready'?tr('model_editor.preflight.ready','LOD0 ТЕХНИЧЕСКИ ГОТОВ К АНАЛИЗУ LOD'):tr('model_editor.preflight.not_ready','LOD0 НЕ ГОТОВ ТЕХНИЧЕСКИ')} · Invalid=${m[2]} · ${tr('model_editor.preflight.classify_count','тип')}=${m[3]}`;
 m=text.match(/^Topology class: LOD(\d+) G(\d+) → (.+)$/);if(m)return `${tr('model_editor.preflight.class','тип')}: LOD${m[1]} G${m[2]} → ${m[3]}`;
 m=text.match(/^Additional LOD meshes refreshed: (\d+) added, (\d+) updated, (\d+) unchanged, (\d+) failed across (\d+) loaded LOD\(s\)(.*)$/);if(m)return `${tr('model_editor.server.extras_refreshed','Исходный набор обновлён')}: +${m[1]} · ${tr('model_editor.server.updated','обновлено')} ${m[2]} · ${tr('model_editor.server.unchanged','без изменений')} ${m[3]} · ${tr('model_editor.server.failed','ошибок')} ${m[4]} · LOD ${m[5]}${m[6]||''}`;
 const prefixPatterns=[
  [/^Added semantic node: (.+)$/, 'model_editor.server.semantic_node_added', ['id']],
  [/^Updated node transform: (.+)$/, 'model_editor.server.node_transform_updated', ['id']],
  [/^Updated default semantic state: (.+) \/ (.+)$/, 'model_editor.server.default_state_updated', ['id','state']],
  [/^Added semantic state variant: (.+)$/, 'model_editor.server.state_variant_added', ['id']],
  [/^Updated semantic state variant: (.+)$/, 'model_editor.server.state_variant_updated', ['id']],
  [/^Deleted semantic state variant: (.+)$/, 'model_editor.server.state_variant_deleted', ['id']],
  [/^Updated render state selector: (.+?)(?: in .*|$)/, 'model_editor.server.render_selector_updated', ['id']],
  [/^Updated joint: (.+)$/, 'model_editor.server.joint_updated', ['id']],
  [/^Updated rigid-body properties: (.+)$/, 'model_editor.server.rigid_updated', ['id']],
  [/^Estimated rigid-body properties from collision: (.+)$/, 'model_editor.server.rigid_estimated', ['id']],
  [/^Added collision volume: (.+)$/, 'model_editor.server.collision_added', ['id']],
  [/^Deleted collision volume: (.+)$/, 'model_editor.server.collision_deleted', ['id']],
  [/^Duplicated collision volume: (.+)$/, 'model_editor.server.collision_duplicated', ['id']],
  [/^Updated collision volume: (.+)$/, 'model_editor.server.collision_updated', ['id']],
  [/^Generated (\d+) radial collision capsules for (.+)$/, 'model_editor.server.collision_ring_generated', ['count','id']],
  [/^Added socket: (.+)$/, 'model_editor.server.socket_added', ['id']],
  [/^Deleted socket: (.+)$/, 'model_editor.server.socket_deleted', ['id']],
  [/^Updated socket: (.+)$/, 'model_editor.server.socket_updated', ['id']],
  [/^Added structural link: (.+)$/, 'model_editor.server.structural_link_added', ['id']],
  [/^Updated structural link: (.+)$/, 'model_editor.server.structural_link_updated', ['id']],
  [/^Deleted structural link: (.+)$/, 'model_editor.server.structural_link_deleted', ['id']],
  [/^Updated structural damage proxy: (.+)$/, 'model_editor.server.structural_proxy_updated', ['id']],
  [/^Updated hit region: (.+)$/, 'model_editor.server.hit_updated', ['id']],
  [/^Deleted hit region: (.+)$/, 'model_editor.server.hit_deleted', ['id']],
  [/^Updated opening: (.+)$/, 'model_editor.server.opening_updated', ['id']],
  [/^Deleted opening: (.+)$/, 'model_editor.server.opening_deleted', ['id']],
  [/^Updated repair target: (.+)$/, 'model_editor.server.repair_updated', ['id']],
  [/^Deleted repair target: (.+)$/, 'model_editor.server.repair_deleted', ['id']],
 ];
 for(const [pattern,key,names] of prefixPatterns){m=text.match(pattern);if(m){const params={};names.forEach((name,index)=>params[name]=m[index+1]);return tr(key,text,params);}}
 if(text==='CLEAN LEGACY SEMANTICS: no recognized module->visual semantic bootstrap nodes found')return tr('model_editor.server.legacy_semantics_none',text);
 m=text.match(/^CLEAN LEGACY SEMANTICS: removed (\d+) synthetic visual semantic node\(s\), rebound (\d+) RenderNode binding\(s\); transform parent chains and structural graph links were not changed$/);if(m)return tr('model_editor.server.legacy_semantics_cleaned',text,{removed:m[1],rebound:m[2]});
 if(text==='STATIC TREE -> ASSET SPACE: no safe static transform edges found')return tr('model_editor.server.static_flatten_none',text);
 if(text==='STATIC TREE -> ASSET SPACE: nothing changed')return tr('model_editor.server.static_flatten_unchanged',text);
 m=text.match(/^STATIC TREE -> ASSET SPACE: moved (\d+) semantic part\(s\) to asset space without changing world pose; resident render geometry verified unchanged; STRUCTURAL GRAPH unchanged$/);if(m)return tr('model_editor.server.static_flatten_done',text,{count:m[1]});
 m=text.match(/^(Reparented|Reordered) (\d+) semantic subtree root\(s\) \(([^)]+)\) (without changing world pose|in editor tree only)$/);if(m)return tr(m[1]==='Reparented'?'model_editor.server.semantic_reparented':'model_editor.server.semantic_reordered',text,{count:m[2],placement:m[3]});
 m=text.match(/^Updated semantic binding for render node '(.+)' in (\d+) LOD\(s\)(.*)$/);if(m)return tr('model_editor.server.semantic_binding_updated',text,{id:m[1],scope:`${m[2]} LOD${m[3]||''}`});
 if(text==='Added state-scoped hit region')return tr('model_editor.server.hit_added',text);
 if(text==='Added state-scoped opening')return tr('model_editor.server.opening_added',text);
 if(text==='Added repair target')return tr('model_editor.server.repair_added',text);
 return text;
}
function languageName(languages,currentLocale,locale){const map=languages?.[locale];return map?.[currentLocale]||map?.[baseLocale(currentLocale)]||map?.en||locale;}
async function loadI18n(){try{const r=await fetch('/model_asset_editor_i18n.json',{cache:'no-store'});if(!r.ok)throw new Error(`${r.status}`);state.i18n=await r.json();if(Array.isArray(state.i18n.locale_order)&&state.i18n.locale_order.length)state.localeOrder=state.i18n.locale_order.slice();}catch(e){console.warn('ModelAssetEditor i18n load failed',e);}uiChrome.setTranslator?.(tr);applyLocale(userLocaleOverride?state.locale:(state.settings?.locale||state.locale));}
function setText(id,key,fallback){const e=$(id);if(e)e.textContent=tr(key,fallback);}
function positionToolTip(el){const tip=$('toolTip'),r=el.getBoundingClientRect();tip.style.left=Math.max(8,Math.min(window.innerWidth-tip.offsetWidth-8,r.left+r.width/2-tip.offsetWidth/2))+'px';tip.style.top=Math.min(window.innerHeight-tip.offsetHeight-8,r.bottom+7)+'px';}
function bindToolTipElement(el){if(!el||el.dataset.tipBound==='1')return;el.dataset.tipBound='1';const show=()=>{const title=tr(el.dataset.tipTitleKey,el.dataset.tipTitle||el.getAttribute('aria-label')||'');const desc=tr(el.dataset.tipDescKey,el.dataset.tipDesc||'');$('toolTipTitle').textContent=title;$('toolTipDesc').textContent=desc;$('toolTip').classList.add('visible');requestAnimationFrame(()=>positionToolTip(el));};const hide=()=>$('toolTip').classList.remove('visible');el.addEventListener('mouseenter',show);el.addEventListener('mouseleave',hide);el.addEventListener('focus',show);el.addEventListener('blur',hide);}
function attachDynamicToolTip(el,titleKey,descKey,titleFallback='',descFallback=''){if(!el)return el;el.dataset.tipTitleKey=titleKey;el.dataset.tipDescKey=descKey;el.dataset.tipTitle=titleFallback;el.dataset.tipDesc=descFallback;el.setAttribute('aria-label',tr(titleKey,titleFallback));el.title=tr(descKey,descFallback);bindToolTipElement(el);return el;}
function applyTooltips(){document.querySelectorAll('[data-tip-title-key]').forEach(el=>{const title=tr(el.dataset.tipTitleKey,el.getAttribute('aria-label')||el.dataset.tipTitle||'');const desc=tr(el.dataset.tipDescKey,el.dataset.tipDesc||'');el.setAttribute('aria-label',title);el.dataset.tipTitle=title;el.dataset.tipDesc=desc;el.title=desc;bindToolTipElement(el);});$('edgeTarget').title=tr('model_editor.edge.selector_hint','Choose which authored edge mask is being edited.');}
function populateLanguageSelect(){const sel=$('settingsLanguage');if(!sel)return;const selected=sel.value||state.locale;sel.innerHTML=state.localeOrder.map(l=>`<option value="${l}">${languageName(state.i18n?.languages||{},state.locale,l)}</option>`).join('');sel.value=state.localeOrder.includes(selected)?selected:state.locale;}
function applyLocale(locale=state.locale){state.locale=state.localeOrder.includes(locale)?locale:(state.i18n?.default_locale||'en');document.documentElement.lang=state.locale;applyDocumentTranslations(document);applyTooltips();populateLanguageSelect();uiChrome.setTranslator?.(tr);uiChrome.refresh?.();refreshDynamicUi();applyDocumentTranslations(document);return state.locale;}
function flushPendingLocale(){if(!pendingLocalePersist||!state.settings)return false;state.settings.locale=state.locale;send('set_locale',{locale:state.locale},{modal:false,status:false});pendingLocalePersist=false;return true;}
function persistLocale(locale){userLocaleOverride=true;pendingLocalePersist=true;applyLocale(locale);flushPendingLocale();}
function acceptSettingsLocale(locale){if(userLocaleOverride){pendingLocalePersist=true;applyLocale(state.locale);flushPendingLocale();return state.locale;}return applyLocale(locale);}
function cycleLocale(){const i=Math.max(0,state.localeOrder.indexOf(state.locale));persistLocale(state.localeOrder[(i+1)%state.localeOrder.length]);localStatus(`${tr('model_editor.settings.language','Interface language')}: ${languageName(state.i18n?.languages||{},state.locale,state.locale)}`);}
return {tr,translateServerMessage,loadI18n,bindToolTipElement,attachDynamicToolTip,populateLanguageSelect,applyLocale,applyDocumentTranslations,acceptSettingsLocale,cycleLocale};
};

export {createI18nEffects};