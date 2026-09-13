// Model Asset Editor effect adapter: shared high-tech UI chrome / contextual help.
import chromeModel from '../ui/chrome_model.js';

export default (()=>{
  const install=({document:doc=globalThis.document,window:win=globalThis.window}={})=>{
    if(!doc||!win)return Object.freeze({refresh:()=>{},dispose:()=>{}});
    if(doc.documentElement.dataset.modelAssetUiChrome==='1')return globalThis.__eliteModelAssetUiChrome||Object.freeze({refresh:()=>{},dispose:()=>{}});
    doc.documentElement.dataset.modelAssetUiChrome='1';

    const helpEntries=new Map();
    let refreshQueued=false;
    let observer=null;
    const q=(selector,root=doc)=>root?.querySelector?.(selector)||null;
    const qa=(selector,root=doc)=>[...(root?.querySelectorAll?.(selector)||[])];
    const normalize=chromeModel.normalizeText;
    const stageName=()=>q('.wizardStage.current')?.dataset?.wizardStage||q('.wizardStage[aria-current="step"]')?.dataset?.wizardStage||'';
    const locale=()=>doc.documentElement.lang||'en';
    const unique=items=>[...new Set(items.map(normalize).filter(Boolean))];
    const isOperational=node=>chromeModel.keepVisible(node?.textContent||'',node?.className||'');
    const hiddenHintSelector='.wizardLead,.sectionHint,.semanticHint,.variantAssignHint,.structGraphHint,.semanticTreeDropHint,.dialogExplanation,.axisRotateHint,.settingsHint';
    const panelSelector=[
      '.geometryToolBlock','.surfaceToolBlock','.semanticTreeBlock','.semanticNodeBlock','.semanticBindingBlock',
      '.semanticMotionBlock','.semanticGraphControls','.modelPreflightBlock','.lodGeneratorBlock','.variantAssignBlock',
      '.geometryEditBlock','.maintenanceBlock','.surfaceBrowser','.compareBlock','.inspectorGroup','.structGraphMeshBlock',
      '.physicalSizeBlock','details.advancedBlock'
    ].join(',');
    const headerSelector=':scope > .geometryBlockHead,:scope > .variantAssignHead,:scope > .surfaceBrowserHead,:scope > .title,:scope > .inspectorGroupTitle,:scope > summary';
    const fallbackTitle=node=>normalize(q(':scope > .title',node)?.textContent||q(':scope > .geometryBlockHead',node)?.textContent||q(':scope > .variantAssignHead',node)?.textContent||q(':scope > .surfaceBrowserHead',node)?.textContent||q(':scope > .inspectorGroupTitle',node)?.textContent||q(':scope > summary',node)?.textContent||'');
    const helpButton=key=>{
      const button=doc.createElement('button');
      button.type='button';
      button.className='uiChromeHelpButton';
      button.dataset.uiChromeHelp=key;
      button.textContent='?';
      button.setAttribute('aria-label','Help');
      button.title='Help';
      return button;
    };
    const collectHints=(owner,directOnly=false)=>{
      const nodes=directOnly?qa(':scope > .wizardLead,:scope > .sectionHint,:scope > .semanticHint,:scope > .variantAssignHint,:scope > .structGraphHint,:scope > .semanticTreeDropHint',owner):qa(hiddenHintSelector,owner);
      const items=[];
      for(const node of nodes){
        if(node.closest('#uiChromeHelpModal'))continue;
        const text=normalize(node.textContent);
        if(!text)continue;
        if(isOperational(node)){node.classList.remove('uiChromeHelpSource');continue;}
        items.push(text);
        node.classList.add('uiChromeHelpSource');
      }
      return unique(items);
    };
    const register=(owner,header,key,title,fallbackKey,directOnly=false)=>{
      if(!owner||!header||!key)return;
      const resolvedTitle=header.dataset.uiChromeTitle||normalize(title)||chromeModel.topicFor(fallbackKey,locale()).title;
      if(!header.dataset.uiChromeTitle)header.dataset.uiChromeTitle=resolvedTitle;
      if(!header.classList.contains('uiChromeHeader'))header.classList.add('uiChromeHeader');
      let button=header.querySelector(':scope > .uiChromeHelpButton');
      if(!button){button=helpButton(key);header.appendChild(button);}else button.dataset.uiChromeHelp=key;
      const items=collectHints(owner,directOnly);
      helpEntries.set(key,{title:resolvedTitle,items,fallbackKey});
    };
    const panelKey=(stage,index,title)=>`panel:${stage||'global'}:${index}:${normalize(title).toLowerCase().replace(/[^a-z0-9а-яё]+/gi,'-').replace(/^-|-$/g,'').slice(0,48)}`;
    const ensureSyntheticHeader=(panel,title)=>{
      let header=q(':scope > .uiChromeSyntheticHead',panel);
      if(!header){
        header=doc.createElement('div');
        header.className='uiChromeSyntheticHead uiChromeHeader';
        const label=doc.createElement('span');label.className='grow';label.textContent=title||'TOOLS';header.appendChild(label);panel.prepend(header);
      }
      return header;
    };
    const decorateWizard=()=>{
      const root=q('#wizardPanel');if(!root)return;
      const stage=stageName();
      const title=q(':scope > .title',root);
      if(title)register(root,title,`stage:${stage}`,title.dataset.uiChromeTitle||normalize(title.textContent),`stage:${stage}`,true);
      let index=0;
      for(const panel of qa(panelSelector,root)){
        let header=q(headerSelector,panel);
        const label=header?.dataset?.uiChromeTitle||fallbackTitle(panel)||normalize(q(':scope > b:first-child',panel)?.textContent)||normalize(q(':scope > .grow:first-child',panel)?.textContent)||`TOOLS ${index+1}`;
        if(!header)header=ensureSyntheticHeader(panel,label);
        const key=panelKey(stage,index++,label);
        register(panel,header,key,label,`stage:${stage}`,false);
      }
    };
    const decorateStaticSections=()=>{
      for(const section of qa('#side .scroll > .section')){
        if(section.id==='wizardPanel'||section.id==='mainHelp')continue;
        const header=q(':scope > .title',section);if(!header)continue;
        register(section,header,`section:${section.id}`,header.dataset.uiChromeTitle||normalize(header.textContent),`section:${section.id}`,false);
      }
      const sideHead=q('#side > .sideHead');
      if(sideHead){
        sideHead.classList.add('uiChromeHeader');
        let button=q(':scope > .uiChromeHelpButton',sideHead);
        if(!button){button=helpButton('section:editor');sideHead.appendChild(button);}
        helpEntries.set('section:editor',{title:normalize(q('#assetTitle')?.textContent)||'MODEL ASSET EDITOR',items:[],fallbackKey:'section:editor'});
      }
      const mainHelp=q('#mainHelp');
      if(mainHelp){
        const text=normalize(mainHelp.textContent);
        if(text)helpEntries.set('section:editor',{...(helpEntries.get('section:editor')||{}),items:[text],fallbackKey:'section:editor'});
        mainHelp.classList.add('uiChromeHelpSource');
      }
    };
    const refresh=()=>{refreshQueued=false;decorateWizard();decorateStaticSections();};
    const scheduleRefresh=()=>{if(refreshQueued)return;refreshQueued=true;(win.requestAnimationFrame||win.setTimeout)(refresh,0);};
    const openHelp=key=>{
      const modal=q('#uiChromeHelpModal');if(!modal)return;
      const entry=helpEntries.get(key)||{title:'',items:[],fallbackKey:key};
      const fallback=chromeModel.topicFor(entry.fallbackKey||key,locale());
      q('#uiChromeHelpTitle').textContent=entry.title||fallback.title||'HELP';
      q('#uiChromeHelpIntro').textContent=fallback.intro||'';
      const list=q('#uiChromeHelpList');
      const items=unique(entry.items||[]);
      list.replaceChildren(...items.map(text=>{const li=doc.createElement('li');li.textContent=text;return li;}));
      list.classList.toggle('hidden',items.length===0);
      modal.classList.remove('hidden');
      q('#uiChromeHelpClose')?.focus();
    };
    const closeHelp=()=>q('#uiChromeHelpModal')?.classList.add('hidden');

    if(!q('#uiChromeStyle')){
      const style=doc.createElement('style');
      style.id='uiChromeStyle';
      style.textContent=`
      :root{--ui-cyan:#7fd6ff;--ui-cyan2:#3b8fbd;--ui-line:#2b455d;--ui-panel:#0b141d;--ui-panel2:#101d2a;--ui-glow:#0b2940}
      button,select,input{font-family:Consolas,'Courier New',monospace}
      button:not(.uiChromeHelpButton):not(.previewToggle){border-radius:2px!important;border-color:#345978!important;background:linear-gradient(180deg,#14283b 0%,#0b151f 100%)!important;box-shadow:inset 0 0 0 1px #172b3b,0 0 0 transparent!important;clip-path:polygon(6px 0,100% 0,100% calc(100% - 6px),calc(100% - 6px) 100%,0 100%,0 6px);letter-spacing:.035em;transition:border-color .12s ease,box-shadow .12s ease,background .12s ease,color .12s ease}
      button:not(.uiChromeHelpButton):not(.previewToggle):hover:not(:disabled){border-color:var(--ui-cyan)!important;background:linear-gradient(180deg,#1a3852 0%,#0d1b28 100%)!important;box-shadow:inset 0 0 0 1px #28516d,0 0 14px #0a2437!important;color:#e9f8ff!important}
      button.active:not(.uiChromeHelpButton),button.wizardPrimary,button.wizardComplete{border-color:#5d9cca!important;box-shadow:inset 0 0 0 1px #2b5876,0 0 10px #0a2030!important}
      button.dangerTool,.sourceDeleteConfirm{border-color:#80505b!important;background:linear-gradient(180deg,#341920,#180d11)!important;color:#ffb7be!important}
      button:disabled{filter:saturate(.3);opacity:.34!important}
      select,input:not([type=checkbox]):not([type=range]){border-radius:2px!important;border-color:#2f506b!important;background:#0a141e!important;box-shadow:inset 0 0 0 1px #111f2c;color:#d9ebf7}
      select:focus,input:focus{outline:1px solid #5a9dcc!important;box-shadow:0 0 10px #0b2a40!important}
      .wizardStage{min-height:31px!important;border-color:#2b465f!important;background:linear-gradient(180deg,#101e2b,#0a131b)!important;color:#8da9bd!important;text-transform:uppercase;letter-spacing:.07em}
      .wizardStage.current{border-color:#a6ecff!important;background:linear-gradient(180deg,#1b4e70,#0d2536)!important;color:#f3fbff!important;box-shadow:inset 0 -3px #7de0ff,inset 0 0 18px #0d3954,0 0 16px #092a40!important}
      #wizardPanel,.section{background:linear-gradient(180deg,#0e1822,#0a1118)}
      .geometryToolBlock,.surfaceToolBlock,.semanticTreeBlock,.semanticNodeBlock,.semanticBindingBlock,.semanticMotionBlock,.semanticGraphControls,.modelPreflightBlock,.lodGeneratorBlock,.variantAssignBlock,.geometryEditBlock,.maintenanceBlock,.surfaceBrowser,.compareBlock,.inspectorGroup,.structGraphMeshBlock,.physicalSizeBlock,.sourceInventory,.advancedBlock{border-color:#29445b!important;border-radius:7px!important;background:linear-gradient(180deg,#0d1822,#091119)!important;box-shadow:inset 0 0 0 1px #111f2b,0 5px 16px #0004}
      .geometryBlockHead,.variantAssignHead,.surfaceBrowserHead,.uiChromeSyntheticHead,.uiChromeHeader{background:linear-gradient(180deg,#122436,#0d1925)!important;border-color:#29445b!important}
      .title,.geometryBlockHead .grow,.variantAssignHead .grow,.surfaceBrowserHead .grow,.inspectorGroupTitle{color:#a9dcf7!important;letter-spacing:.045em;text-transform:uppercase}
      .uiChromeHeader{display:flex!important;align-items:center!important;gap:7px!important;min-width:0}
      .uiChromeHeader>.grow,.uiChromeHeader>span:first-child,.uiChromeHeader>b:first-child{flex:1;min-width:0}
      .uiChromeHelpButton{display:inline-grid!important;place-items:center;width:24px!important;height:24px!important;min-width:24px!important;min-height:24px!important;padding:0!important;border:1px solid #4b7da1!important;border-radius:50%!important;background:radial-gradient(circle at 35% 30%,#193e59,#09131d 72%)!important;color:#c9efff!important;font:700 13px/1 Consolas,monospace!important;box-shadow:inset 0 0 0 1px #1d4057,0 0 10px #08263a!important;clip-path:none!important;cursor:pointer}
      .uiChromeHelpButton:hover{border-color:#9ce4ff!important;color:#fff!important;box-shadow:inset 0 0 0 1px #3b7598,0 0 16px #0c4a70!important}
      .uiChromeSyntheticHead{display:flex;align-items:center;gap:7px;padding:8px;border-bottom:1px solid #29445b;color:#a9dcf7;font:700 10px Arial,sans-serif;letter-spacing:.05em;text-transform:uppercase}
      .uiChromeHelpSource{display:none!important}
      #side>.sideHead{position:relative;padding-right:42px!important}
      #side>.sideHead>.uiChromeHelpButton{position:absolute;right:9px;top:8px}
      .badge{border-color:#355871!important;background:#0b1721;color:#9fcde8}
      .wizardOk,.surfaceReady,.maintenanceClean{border-color:#2b6e4a!important;background:#091d14!important;color:#8be2aa!important}
      .wizardWarning,.surfaceWarn,.statusBanner.danger,.surfaceIssues{border-color:#7d5633!important;background:#23180c!important;color:#f0c17d!important}
      #uiChromeHelpModal{position:fixed;inset:0;z-index:520;display:flex;align-items:center;justify-content:center;padding:24px;background:#02060bd9;backdrop-filter:blur(4px)}
      #uiChromeHelpModal.hidden{display:none!important}
      .uiChromeHelpPanel{width:min(720px,88vw);max-height:min(760px,86vh);overflow:hidden;border:1px solid #4b7da1;border-radius:8px;background:linear-gradient(180deg,#101e2b,#080f16);box-shadow:0 20px 70px #000d,0 0 28px #08283e}
      .uiChromeHelpHead{display:flex;align-items:center;gap:10px;padding:11px 13px;border-bottom:1px solid #29445b;background:linear-gradient(180deg,#163047,#0d1c29)}
      #uiChromeHelpTitle{flex:1;color:#d8f2ff;font:700 13px Consolas,monospace;letter-spacing:.06em;text-transform:uppercase}
      #uiChromeHelpClose{width:32px;height:28px;padding:0!important}
      .uiChromeHelpBody{padding:14px 16px 17px;overflow:auto;max-height:calc(86vh - 52px);color:#abc0d1;line-height:1.5}
      #uiChromeHelpIntro{margin:0 0 12px;color:#9ac6df}
      #uiChromeHelpList{margin:0;padding-left:20px;color:#d4e5f1}
      #uiChromeHelpList li+li{margin-top:8px}
      `;
      doc.head.appendChild(style);
    }
    if(!q('#uiChromeHelpModal')){
      const modal=doc.createElement('div');
      modal.id='uiChromeHelpModal';modal.className='hidden';modal.setAttribute('role','dialog');modal.setAttribute('aria-modal','true');
      modal.innerHTML='<div class="uiChromeHelpPanel"><div class="uiChromeHelpHead"><span id="uiChromeHelpTitle">HELP</span><button id="uiChromeHelpClose" type="button">×</button></div><div class="uiChromeHelpBody"><p id="uiChromeHelpIntro"></p><ul id="uiChromeHelpList"></ul></div></div>';
      doc.body.appendChild(modal);
    }
    doc.addEventListener('click',event=>{
      const button=event.target?.closest?.('[data-ui-chrome-help]');
      if(button){event.preventDefault();event.stopPropagation();openHelp(String(button.dataset.uiChromeHelp||''));return;}
      if(event.target?.id==='uiChromeHelpClose'||event.target?.id==='uiChromeHelpModal')closeHelp();
    });
    doc.addEventListener('keydown',event=>{if(event.key==='Escape')closeHelp();});
    const Observer=win.MutationObserver||globalThis.MutationObserver;
    if(Observer){observer=new Observer(scheduleRefresh);const observed=q('#side')||doc.body;observer.observe(observed,{subtree:true,childList:true,characterData:true});}
    scheduleRefresh();
    const api=Object.freeze({refresh:scheduleRefresh,dispose:()=>{observer?.disconnect();observer=null;q('#uiChromeStyle')?.remove();q('#uiChromeHelpModal')?.remove();delete doc.documentElement.dataset.modelAssetUiChrome;}});
    globalThis.__eliteModelAssetUiChrome=api;
    return api;
  };
  const api=Object.freeze({install});
  if(globalThis.document)install();
  return api;
})();
