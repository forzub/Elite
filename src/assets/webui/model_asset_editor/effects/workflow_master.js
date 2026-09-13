// Model Asset Editor effect adapter: workflow master / progressive contextual help.
import chromeModel from '../ui/chrome_model.js';
import {compactLabel,normalizeText,workflowDefinitionFor,workflowMasterHtml} from '../ui/workflow_master_model.js';
import {WORKFLOW_MASTER_CSS} from '../ui/workflow_master_style.js';

const installModelAssetWorkflowMaster=(doc=globalThis.document,win=globalThis.window)=>{
  if(!doc||!win)return Object.freeze({refresh:()=>{},dispose:()=>{}});
  if(doc.documentElement.dataset.modelAssetWorkflowMaster==='1')return globalThis.__eliteModelAssetWorkflowMaster||Object.freeze({refresh:()=>{},dispose:()=>{}});
  doc.documentElement.dataset.modelAssetWorkflowMaster='1';

  const helpEntries=new Map();
  let queued=false;
  let observer=null;
  const q=(selector,root=doc)=>root?.querySelector?.(selector)||null;
  const qa=(selector,root=doc)=>[...(root?.querySelectorAll?.(selector)||[])];
  const stageName=()=>q('.wizardStage.current')?.dataset?.wizardStage||q('.wizardStage[aria-current="step"]')?.dataset?.wizardStage||'';
  const structureMode=()=>q('input[name="semanticStructureMode"]:checked')?.value==='graph'?'graph':'tree';
  const unique=values=>[...new Set(values.map(normalizeText).filter(Boolean))];

  const ensureStyle=()=>{
    if(q('#modelAssetWorkflowMasterStyle'))return;
    const style=doc.createElement('style');style.id='modelAssetWorkflowMasterStyle';style.textContent=WORKFLOW_MASTER_CSS;doc.head.appendChild(style);
  };
  const ensureModal=()=>{
    if(q('#workflowHelpModal'))return;
    const modal=doc.createElement('div');
    modal.id='workflowHelpModal';modal.className='hidden';modal.setAttribute('role','dialog');modal.setAttribute('aria-modal','true');
    modal.innerHTML='<div class="workflowHelpPanel"><div class="workflowHelpHead"><span id="workflowHelpTitle">HELP</span><button id="workflowHelpClose" type="button">×</button></div><div class="workflowHelpBody"><p id="workflowHelpIntro"></p><ul id="workflowHelpList"></ul></div></div>';
    doc.body.appendChild(modal);
  };
  const openHelp=key=>{
    const entry=helpEntries.get(key);if(!entry)return;
    q('#workflowHelpTitle').textContent=entry.title||'HELP';
    q('#workflowHelpIntro').textContent=entry.intro||'';
    const list=q('#workflowHelpList'),items=unique(entry.items||[]);
    list.replaceChildren(...items.map(value=>{const li=doc.createElement('li');li.textContent=value;return li;}));
    list.classList.toggle('hidden',items.length===0);
    q('#workflowHelpModal').classList.remove('hidden');q('#workflowHelpClose')?.focus();
  };
  const closeHelp=()=>q('#workflowHelpModal')?.classList.add('hidden');
  const makeHelpButton=key=>{
    const button=doc.createElement('button');button.type='button';button.className='workflowHelpButton';button.dataset.workflowHelp=key;button.textContent='?';button.title='Help';button.setAttribute('aria-label','Help');return button;
  };
  const targetLabel=(step,target,index)=>{
    const selector=String(step.labelFrom||'');
    let node=null;
    if(selector){try{node=target?.matches?.(selector)?target:q(selector,target)||q(selector);}catch(_){node=null;}}
    const fallback=String(step.id||`step-${index+1}`).split('-').slice(1).join(' ').toUpperCase()||`STEP ${index+1}`;
    return compactLabel(node?.textContent||target?.getAttribute?.('aria-label')||target?.title||'',fallback);
  };
  const workflowHost=stage=>{
    const panel=q('#wizardPanel');if(!panel)return null;
    return q(`.${stage==='semantics'?'semantic':stage==='surfaces'?'surface':stage==='geometry'?'geometry':'__none__'}Workspace`,panel)||panel;
  };
  const scrollTarget=selector=>{
    let target=null;try{target=q(selector);}catch(_){return;}
    if(!target)return;
    const focusTarget=target.closest?.('.geometryToolBlock,.surfaceToolBlock,.semanticTreeBlock,.semanticNodeBlock,.semanticBindingBlock,.semanticMotionBlock,.semanticGraphControls,.modelPreflightBlock,.lodGeneratorBlock,.variantAssignBlock,.geometryEditBlock,.maintenanceBlock,.surfaceBrowser,.structGraphMeshBlock,.section')||target;
    focusTarget.scrollIntoView?.({behavior:'smooth',block:'center',inline:'nearest'});
    focusTarget.classList.add('workflowTargetFocus');win.setTimeout(()=>focusTarget.classList.remove('workflowTargetFocus'),1250);
    if(target.matches?.('button,input,select,[tabindex]'))target.focus?.({preventScroll:true});
  };

  const registerOwnerHelp=(owner,title,items,key)=>{
    const clean=unique(items);if(!owner||!clean.length)return;
    helpEntries.set(key,{title:compactLabel(title,'HELP'),intro:'',items:clean});
    if(owner.querySelector(':scope > .uiChromeHelpButton,:scope > .workflowHelpButton'))return;
    owner.classList.add('workflowHelpOwner');owner.appendChild(makeHelpButton(key));
  };
  const hideExplanatoryCopy=stage=>{
    const selectors='.wizardLead,.sectionHint,.semanticHint,.variantAssignHint,.structGraphHint,.semanticTreeDropHint,.semanticPreviewNote';
    let ownerSerial=0;
    for(const hint of qa(selectors)){
      if(hint.closest('#uiChromeHelpModal,#workflowHelpModal'))continue;
      const text=normalizeText(hint.textContent);if(!text)continue;
      if(chromeModel.keepVisible(text,hint.className)){hint.classList.remove('workflowHelpSource');continue;}
      hint.classList.add('workflowHelpSource');
      const owner=hint.closest('.wizardLodBlock,.geometryToolBlock,.surfaceToolBlock,.semanticTreeBlock,.semanticNodeBlock,.semanticBindingBlock,.semanticMotionBlock,.semanticGraphControls,.modelPreflightBlock,.lodGeneratorBlock,.variantAssignBlock,.geometryEditBlock,.maintenanceBlock,.surfaceBrowser,.structGraphMeshBlock,.section');
      if(owner&&!owner.querySelector('.uiChromeHelpButton')){
        const title=normalizeText(q(':scope > .geometryBlockHead,:scope > .variantAssignHead,:scope > .surfaceBrowserHead,:scope > .title,:scope > b:first-child',owner)?.textContent)||stage.toUpperCase();
        registerOwnerHelp(owner,title,[text],`hint:${stage}:${ownerSerial++}`);
      }
    }
    const legacy=q('.semanticLegacyCleanup');
    const legacyText=q(':scope > .grow',legacy);
    if(legacy&&legacyText){const text=normalizeText(legacyText.textContent);legacyText.classList.add('workflowHelpSource');registerOwnerHelp(legacy,q('#semanticCleanLegacyTree,#semanticCleanLegacyGraph',legacy)?.textContent||'LEGACY',[text],`semantic:legacy:${structureMode()}`);}
    const selection=q('.semanticTreeSelectionBar');
    if(selection){const help=q(':scope > span:not(.grow)',selection);if(help){const text=normalizeText(help.textContent);help.classList.add('workflowHelpSource');registerOwnerHelp(selection,q('#semanticSelectedCount',selection)?.textContent||'SELECTION',[text],'semantic:selection');}}
  };
  const decorateMaster=()=>{
    const stage=stageName();if(!stage)return;
    const host=workflowHost(stage);if(!host)return;
    const mode=stage==='semantics'?structureMode():'tree';
    const definition=workflowDefinitionFor(stage,mode),steps=[];
    for(let i=0;i<definition.length;i++){
      const step=definition[i];let target=null;try{target=q(step.target);}catch(_){target=null;}
      if(!target)continue;steps.push({...step,label:targetLabel(step,target,i)});
    }
    const title=q(':scope > .title',host)||q('.title',host);if(!title||!steps.length)return;
    const signature=`${stage}:${mode}:`+steps.map(x=>`${x.id}:${x.label}`).join('|');
    let master=q(':scope > .modelAssetWorkflowMaster',host);
    if(!master||master.dataset.workflowSignature!==signature){
      master?.remove();title.insertAdjacentHTML('afterend',workflowMasterHtml(steps));master=q(':scope > .modelAssetWorkflowMaster',host);master.dataset.workflowSignature=signature;
    }
    const stageLead=q(':scope > .wizardLead',host)||q('.wizardLead',host),lead=normalizeText(stageLead?.textContent||'');
    if(stageLead)stageLead.classList.add('workflowHelpSource');
    const stageKey=`stage:${stage}:${mode}`;
    helpEntries.set(stageKey,{title:normalizeText(title.textContent)||stage.toUpperCase(),intro:'',items:lead?[lead]:[]});
    const help=q('[data-workflow-help="stage"]',master);if(help)help.dataset.workflowHelp=stageKey;
    for(const button of qa('[data-workflow-step]',master))button.onclick=()=>scrollTarget(button.dataset.workflowTarget||'');
  };
  const refresh=()=>{queued=false;decorateMaster();hideExplanatoryCopy(stageName()||'editor');};
  const schedule=()=>{if(queued)return;queued=true;(win.requestAnimationFrame||win.setTimeout)(refresh,0);};

  ensureStyle();ensureModal();
  doc.addEventListener('click',event=>{
    const help=event.target?.closest?.('[data-workflow-help]');if(help){event.preventDefault();event.stopPropagation();openHelp(help.dataset.workflowHelp);return;}
    if(event.target?.id==='workflowHelpClose'||event.target?.id==='workflowHelpModal')closeHelp();
  });
  doc.addEventListener('keydown',event=>{if(event.key==='Escape'&&!q('#workflowHelpModal')?.classList.contains('hidden'))closeHelp();});
  doc.addEventListener('change',event=>{if(event.target?.name==='semanticStructureMode')schedule();});
  observer=new MutationObserver(schedule);observer.observe(doc.body,{subtree:true,childList:true,attributes:true,attributeFilter:['class','disabled']});
  schedule();
  const api=Object.freeze({refresh:schedule,dispose:()=>observer?.disconnect()});globalThis.__eliteModelAssetWorkflowMaster=api;return api;
};

const boot=()=>installModelAssetWorkflowMaster(globalThis.document,globalThis.window);
if(globalThis.document){if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',boot,{once:true});else queueMicrotask(boot);}

export {installModelAssetWorkflowMaster};
