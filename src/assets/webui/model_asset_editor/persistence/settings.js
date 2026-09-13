function createSettingsPersistence({state,$,clearSettingsSaveTimer,setSettingsSaving,notice,tr,applyLocale,openSettings,logger}){
 function refreshLocale(locale,context){
  if(!locale||state.locale===locale)return;
  try{applyLocale(locale);}catch(error){logger.error(`${context} locale refresh failed`,error);}
 }

 function handleSettings(msg){
  state.settings=msg;
  if(!state.settingsSaving)refreshLocale(msg.locale,'settings');
  if(!state.settingsSaving&&!$('settingsModal').classList.contains('hidden'))openSettings();
 }

 function handleSettingsSaved(msg){
  clearSettingsSaveTimer();
  state.settingsSaving=false;
  state.settings={...(state.settings||{}),...msg};
  const feedback=$('settingsFeedback');
  if(feedback){
   feedback.textContent='';
   feedback.className='settingsFeedback';
  }
  setSettingsSaving(false);
  $('settingsModal').classList.add('hidden');
  state.ignoreNextStatusNotice=true;
  const source=msg.sourceAssetsRoot||state.settings?.sourceAssetsRoot||'';
  notice(`${tr('model_editor.settings.saved','Settings saved')} · ${source}`,false);
  refreshLocale(msg.locale,'post-save');
 }

 function handleStatusError(localizedMessage){
  if(!state.settingsSaving)return;
  clearSettingsSaveTimer();
  state.settingsSaving=false;
  setSettingsSaving(false);
  const feedback=$('settingsFeedback');
  if(feedback){
   feedback.textContent=localizedMessage||tr('model_editor.status.error','ERROR');
   feedback.className='settingsFeedback error';
  }
 }

 return {handleSettings,handleSettingsSaved,handleStatusError};
}

export {createSettingsPersistence};
