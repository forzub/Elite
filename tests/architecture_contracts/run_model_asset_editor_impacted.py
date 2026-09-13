from pathlib import Path
import argparse,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
TESTDIR=Path(__file__).resolve().parent
CHECKS={
 'app':'check_model_asset_editor_application_state.py','orchestration':'check_model_asset_editor_orchestration_layers.py','transport':'check_model_asset_editor_transport_layers.py','session':'check_model_asset_editor_session_layers.py','viewport':'check_model_asset_editor_viewport_layers.py','viewport_adapters':'check_model_asset_editor_viewport_adapters.py','view_state':'check_model_asset_editor_view_state_layers.py','shell':'check_model_asset_editor_shell_architecture.py','semantics':'check_model_asset_semantics_workspace_layout.py','i18n':'check_model_asset_editor_localization.py'}
def changed(base,explicit):
 if explicit:return explicit
 command=['git','diff','--name-only',f'{base}...HEAD']
 return [x for x in subprocess.check_output(command,cwd=ROOT,text=True).splitlines() if x]
def select(paths):
 chosen={'shell'}
 for path in paths:
  p=path.replace('\\','/')
  if p.endswith('model_asset_editor.html'): chosen.update(CHECKS)
  if '/model_asset_editor/app/' in p: chosen.update({'app','orchestration','view_state','shell'})
  if '/model_asset_editor/effects/workflow.js' in p: chosen.update({'orchestration','shell'})
  if '/model_asset_editor/transport/' in p: chosen.update({'transport','session','shell'})
  if '/model_asset_editor/session/' in p or '/model_asset_editor/persistence/' in p: chosen.update({'session','transport','shell'})
  if '/model_asset_editor/viewport/' in p: chosen.update({'viewport','viewport_adapters','view_state','shell'})
  if '/model_asset_editor/semantics/' in p or '/effects/semantics.js' in p: chosen.update({'semantics','i18n','shell'})
  if '/localization/' in p or '/i18n/' in p or p.endswith('/effects/i18n.js') or p.endswith('/effects/ui_chrome.js') or '/ui/' in p: chosen.update({'i18n','shell'})
  if p.startswith('tools/model_asset_editor/'): chosen.update({'session','i18n','shell'})
 return [name for name in CHECKS if name in chosen]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--base',default='HEAD^');ap.add_argument('files',nargs='*');args=ap.parse_args()
 paths=changed(args.base,args.files); selected=select(paths)
 print('MODEL ASSET EDITOR IMPACTED CONTRACT RUNNER')
 print(' changed files:',len(paths));[print('  -',p) for p in paths]
 print(' selected contracts:',', '.join(selected))
 failed=[]
 for name in selected:
  script=TESTDIR/CHECKS[name]
  if not script.exists(): continue
  print(f'\n=== {name} ===')
  code=subprocess.call([sys.executable,str(script)],cwd=ROOT)
  if code: failed.append(name)
 if failed: print('\nFAILED:',', '.join(failed));return 1
 print('\nIMPACTED ARCHITECTURE CONTRACTS: PASS');return 0
if __name__=='__main__': raise SystemExit(main())
