#!/usr/bin/env python3
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]
HTML=ROOT/'src/assets/webui/model_asset_editor.html'
VERSION=ROOT/'tools/model_asset_editor/EditorVersion.h'
text=HTML.read_text(encoding='utf-8')
version=VERSION.read_text(encoding='utf-8')

# The common CHECK/BUILD action is physically after every stage-specific/shared section.
footer='<div id="wizardStageFooter" class="wizardStageFooter"></div>'
help_node='<div id="mainHelp"'
assert footer in text, 'common wizard stage footer missing'
assert text.index(footer) > text.index(help_node), 'stage footer must be the final control in the side scroll column'
assert 'function renderWizardStageFooter()' in text, 'common footer renderer missing'
assert 'renderWizardStageFooter();return result;' in text, 'wizard panel refresh must also refresh the common footer'

# Exactly one stage CHECK control may exist in the rendered DOM, and shell footer owns it.
assert text.count('id="wizardStageCheckBtn"') == 1, 'stage CHECK button definition must have one owner'
for stage in ['source','lods','geometry','surfaces','physics','damage','validate','build']:
    assert f"wizardStageCheckControls('{stage}')" not in text, f'{stage} renderer still owns CHECK placement'

# Supplemental stage sections must occur before the common footer, so CHECK is visually last.
for section in ['sharedStageMeshSection','semanticSection','statesSection','lodSection','renderAssemblySection','geometrySection','activeLodSection','storageSection','semanticInspectorSection','renderInspectorSection','collisionSection','socketSection','damageSection','materialsSection']:
    marker=f'id="{section}"'
    assert marker in text, f'missing stage section: {section}'
    assert text.index(marker) < text.index(footer), f'{section} appears below stage CHECK footer'

# SEMANTICS mode switch remains top-local, while CHECK is no longer embedded there.
workspace=(ROOT/'src/assets/webui/model_asset_editor/semantics/workspace.js').read_text(encoding='utf-8')
structural=(ROOT/'src/assets/webui/model_asset_editor/semantics/structural.js').read_text(encoding='utf-8')
effects=(ROOT/'src/assets/webui/model_asset_editor/effects/semantics.js').read_text(encoding='utf-8')
assert '<div class="semanticWorkflowBar">${fragments.structureMode}</div>' in workspace
assert '<div class="semanticWorkflowBar">${fragments.structureMode}</div>' in structural
assert 'fragments.stageCheck' not in workspace
assert 'fragments.stageCheck' not in structural
assert "wizardStageCheckControls('semantics')" not in effects

assert re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.86"',version), 'editor version must be 0.10.86'
print('MODEL ASSET EDITOR STAGE FOOTER LAYOUT: PASS')
print(' - SOURCE through BUILD share one bottom-of-tab CHECK/BUILD footer')
print(' - all supplemental authoring sections render above that footer')
print(' - SEMANTICS TREE/GRAPH no longer special-case CHECK at the top')
