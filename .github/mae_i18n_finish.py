from pathlib import Path
import json,re

ROOT=Path(__file__).resolve().parents[1]
HTML=ROOT/'src/assets/webui/model_asset_editor.html'
WEB=ROOT/'src/assets/webui/model_asset_editor'
CAT=ROOT/'src/assets/localization/ui/tools/model_asset_editor.json'
VERSION=ROOT/'tools/model_asset_editor/EditorVersion.h'
TESTS=ROOT/'tests/architecture_contracts'
STATE=ROOT/'CURRENT_STATE.md'
TASK=ROOT/'CURRENT_TASK.md'


def replace_once(text,old,new,label):
    n=text.count(old)
    if n!=1:
        raise RuntimeError(f'{label}: expected 1 match, got {n}')
    return text.replace(old,new,1)

# ---------------------------------------------------------------------------
# Catalog additions for previously hard-coded shell/runtime prose.
# ---------------------------------------------------------------------------
data=json.loads(CAT.read_text(encoding='utf-8'))
S=data.setdefault('strings',{})
add={
'model_editor.document.title':{
 'en':'Elite Model Asset Editor','ru':'Редактор ресурсов моделей Elite','zh-Hans':'Elite 模型资源编辑器','es':'Editor de recursos de modelo Elite','ja':'Elite モデルアセットエディタ'},
'model_editor.status.ready':{
 'en':'Ready','ru':'Готово','zh-Hans':'就绪','es':'Listo','ja':'準備完了'},
'model_editor.geometry_stats.render_nodes':{
 'en':'render nodes','ru':'узлы рендера','zh-Hans':'渲染节点','es':'nodos de renderizado','ja':'レンダーノード'},
'model_editor.geometry_stats.geometry_definitions':{
 'en':'geometry definitions','ru':'описания геометрии','zh-Hans':'几何定义','es':'definiciones de geometría','ja':'ジオメトリ定義'},
'model_editor.geometry_stats.geometry_bindings':{
 'en':'geometry bindings','ru':'привязки геометрии','zh-Hans':'几何绑定','es':'enlaces de geometría','ja':'ジオメトリバインド'},
'model_editor.geometry_stats.shared_groups':{
 'en':'shared geometry groups','ru':'группы общей геометрии','zh-Hans':'共享几何组','es':'grupos de geometría compartida','ja':'共有ジオメトリグループ'},
'model_editor.geometry_stats.additional_meshes':{
 'en':'additional meshes','ru':'дополнительные меши','zh-Hans':'附加网格','es':'mallas adicionales','ja':'追加メッシュ'},
'model_editor.geometry_stats.unused_geometry':{
 'en':'unused ordinary geometry','ru':'неиспользуемая обычная геометрия','zh-Hans':'未使用的普通几何','es':'geometría ordinaria sin usar','ja':'未使用の通常ジオメトリ'},
'model_editor.geometry_stats.vertices':{
 'en':'vertices','ru':'вершины','zh-Hans':'顶点','es':'vértices','ja':'頂点'},
'model_editor.geometry_stats.triangles':{
 'en':'triangles','ru':'треугольники','zh-Hans':'三角形','es':'triángulos','ja':'三角形'},
'model_editor.geometry_variants.no_used_default':{
 'en':'No used default geometry in this LOD.','ru':'В этом LOD нет используемой основной геометрии.','zh-Hans':'此 LOD 中没有正在使用的默认几何。','es':'No hay geometría predeterminada en uso en este LOD.','ja':'この LOD には使用中の既定ジオメトリがありません。'},
'model_editor.status.surface_selected':{
 'en':'SURFACES: LOD{lod} · {count} mesh families selected · G{geometry} · {id}','ru':'ПОВЕРХНОСТИ: LOD{lod} · выбрано семейств мешей: {count} · G{geometry} · {id}','zh-Hans':'表面：LOD{lod} · 已选择 {count} 个网格族 · G{geometry} · {id}','es':'SUPERFICIES: LOD{lod} · {count} familias de malla seleccionadas · G{geometry} · {id}','ja':'サーフェス: LOD{lod} · {count} 個のメッシュ系統を選択 · G{geometry} · {id}'},
'model_editor.status.active_lod':{
 'en':'{stage}: LOD{lod}','ru':'{stage}: LOD{lod}','zh-Hans':'{stage}：LOD{lod}','es':'{stage}: LOD{lod}','ja':'{stage}: LOD{lod}'},
'model_editor.axis_legend.game_frame':{
 'en':'GAME FRAME','ru':'СИСТЕМА КООРДИНАТ ИГРЫ','zh-Hans':'游戏坐标系','es':'SISTEMA DE COORDENADAS DEL JUEGO','ja':'ゲーム座標系'},
'model_editor.axis_legend.game_frame_fixed':{
 'en':'GAME FRAME · FIXED','ru':'СИСТЕМА КООРДИНАТ ИГРЫ · ФИКСИРОВАННАЯ','zh-Hans':'游戏坐标系 · 固定','es':'SISTEMA DE COORDENADAS DEL JUEGO · FIJO','ja':'ゲーム座標系 · 固定'},
'model_editor.axis_legend.directions':{
 'en':'RIGHT +X · UP +Y · NOSE -Z','ru':'ВПРАВО +X · ВВЕРХ +Y · НОС -Z','zh-Hans':'右 +X · 上 +Y · 舰首 -Z','es':'DERECHA +X · ARRIBA +Y · PROA -Z','ja':'右 +X · 上 +Y · 機首 -Z'},
'model_editor.axis_legend.ground':{
 'en':'GROUND XZ · VERTICAL Y','ru':'ПЛОСКОСТЬ XZ · ВЕРТИКАЛЬ Y','zh-Hans':'地面 XZ · 垂直 Y','es':'PLANO XZ · VERTICAL Y','ja':'地面 XZ · 垂直 Y'},
'model_editor.axis_legend.source_to_game':{
 'en':'LOD{lod} SOURCE → GAME','ru':'LOD{lod} ИСТОЧНИК → ИГРА','zh-Hans':'LOD{lod} 源 → 游戏','es':'LOD{lod} ORIGEN → JUEGO','ja':'LOD{lod} ソース → ゲーム'},
'model_editor.overlay.semantic':{
 'en':'semantic','ru':'семантика','zh-Hans':'语义','es':'semántica','ja':'セマンティック'},
'model_editor.overlay.preview_state':{
 'en':'preview state','ru':'состояние предпросмотра','zh-Hans':'预览状态','es':'estado de vista previa','ja':'プレビュー状態'},
'model_editor.overlay.render':{
 'en':'render LOD{lod}','ru':'рендер LOD{lod}','zh-Hans':'渲染 LOD{lod}','es':'render LOD{lod}','ja':'レンダー LOD{lod}'},
'model_editor.overlay.geometry':{
 'en':'geometry','ru':'геометрия','zh-Hans':'几何','es':'geometría','ja':'ジオメトリ'},
'model_editor.overlay.unbound':{
 'en':'unbound','ru':'не привязано','zh-Hans':'未绑定','es':'sin vincular','ja':'未バインド'},
'model_editor.storage.source_meshes':{
 'en':'source meshes','ru':'исходные меши','zh-Hans':'源网格','es':'mallas de origen','ja':'ソースメッシュ'},
'model_editor.storage.working_manifest':{
 'en':'WORKING manifest','ru':'манифест WORKING','zh-Hans':'WORKING 清单','es':'manifiesto WORKING','ja':'WORKING マニフェスト'},
'model_editor.storage.working_lod':{
 'en':'WORKING LOD{lod} .elmesh','ru':'WORKING LOD{lod} .elmesh','zh-Hans':'WORKING LOD{lod} .elmesh','es':'WORKING LOD{lod} .elmesh','ja':'WORKING LOD{lod} .elmesh'},
'model_editor.storage.working_total':{
 'en':'WORKING package total','ru':'весь пакет WORKING','zh-Hans':'WORKING 包总计','es':'total del paquete WORKING','ja':'WORKING パッケージ合計'},
'model_editor.storage.production_last_build':{
 'en':'PRODUCTION package (last BUILD)','ru':'пакет PRODUCTION (последний BUILD)','zh-Hans':'PRODUCTION 包（上次 BUILD）','es':'paquete PRODUCTION (último BUILD)','ja':'PRODUCTION パッケージ（前回の BUILD）'},
'model_editor.storage.render_payload_estimate':{
 'en':'current render payload estimate','ru':'оценка текущего render payload','zh-Hans':'当前渲染载荷估算','es':'estimación de la carga de renderizado actual','ja':'現在のレンダーペイロード推定値'},
'model_editor.storage.unused_active_lod':{
 'en':'unused active-LOD geometry','ru':'неиспользуемая геометрия активного LOD','zh-Hans':'活动 LOD 的未使用几何','es':'geometría sin usar del LOD activo','ja':'アクティブ LOD の未使用ジオメトリ'},
'model_editor.storage.source_variants':{
 'en':'source variant geometries','ru':'геометрия вариантов источника','zh-Hans':'源变体几何','es':'geometrías de variantes de origen','ja':'ソースバリアントジオメトリ'},
'model_editor.storage.working_path':{
 'en':'WORKING','ru':'WORKING','zh-Hans':'WORKING','es':'WORKING','ja':'WORKING'},
'model_editor.storage.production_path':{
 'en':'PRODUCTION','ru':'PRODUCTION','zh-Hans':'PRODUCTION','es':'PRODUCTION','ja':'PRODUCTION'},
'model_editor.storage.help':{
 'en':'WORKING ASSET is the saved editor resume state. SAVE and RESTORE are the only working-state persistence controls. Production changes only at BUILD. Source OBJ/MTL files are read-only authoring inputs.','ru':'WORKING ASSET — сохранённое состояние редактора для продолжения работы. SAVE и RESTORE — единственные команды сохранения рабочего состояния. Production меняется только при BUILD. Исходные OBJ/MTL доступны только для чтения.','zh-Hans':'WORKING ASSET 是编辑器用于继续工作的已保存状态。SAVE 和 RESTORE 是唯一的工作状态持久化控制。Production 仅在 BUILD 时改变。源 OBJ/MTL 文件是只读制作输入。','es':'WORKING ASSET es el estado guardado para reanudar el editor. SAVE y RESTORE son los únicos controles de persistencia del estado de trabajo. Production solo cambia con BUILD. Los archivos OBJ/MTL de origen son entradas de solo lectura.','ja':'WORKING ASSET は編集作業を再開するための保存状態です。SAVE と RESTORE が作業状態を永続化する唯一の操作です。Production は BUILD 時だけ変更されます。ソース OBJ/MTL は読み取り専用の制作入力です。'},
'model_editor.storage.unsaved_warning':{
 'en':'Unsaved WORKING ASSET changes exist. SAVE keeps them; RESTORE discards them. BUILD writes production only from a saved state.','ru':'Есть несохранённые изменения WORKING ASSET. SAVE сохраняет их, RESTORE отменяет. BUILD пишет production только из сохранённого состояния.','zh-Hans':'存在未保存的 WORKING ASSET 更改。SAVE 会保留它们；RESTORE 会放弃它们。BUILD 只从已保存状态写入 production。','es':'Hay cambios de WORKING ASSET sin guardar. SAVE los conserva; RESTORE los descarta. BUILD escribe production solo desde un estado guardado.','ja':'WORKING ASSET に未保存の変更があります。SAVE は保持し、RESTORE は破棄します。BUILD は保存済み状態からのみ production を書き込みます。'},
'model_editor.storage.legacy_package':{
 'en':'Legacy package still exists','ru':'Старый пакет всё ещё существует','zh-Hans':'旧版包仍然存在','es':'El paquete heredado todavía existe','ja':'旧形式パッケージが残っています'},
'model_editor.common.tris_short':{
 'en':'tris','ru':'треуг.','zh-Hans':'三角面','es':'triáng.','ja':'三角形'},
'model_editor.confirm.delete_unused_geometry':{
 'en':'Delete {count} unused geometry definition(s) from LOD{lod}?\n\n{listing}\n\nEstimated payload removed: ~{bytes}\n\nOnly the active render LOD changes. Other LODs, the semantic manifest and source OBJ files are unchanged until explicitly saved.','ru':'Удалить {count} неиспользуемых описаний геометрии из LOD{lod}?\n\n{listing}\n\nОценка удаляемого payload: ~{bytes}\n\nИзменится только активный render LOD. Другие LOD, semantic manifest и исходные OBJ не меняются до явного сохранения.','zh-Hans':'从 LOD{lod} 删除 {count} 个未使用的几何定义？\n\n{listing}\n\n预计移除载荷：~{bytes}\n\n只会更改活动 render LOD。其他 LOD、semantic manifest 和源 OBJ 文件在显式保存前保持不变。','es':'¿Eliminar {count} definiciones de geometría sin usar de LOD{lod}?\n\n{listing}\n\nCarga estimada eliminada: ~{bytes}\n\nSolo cambia el render LOD activo. Los demás LOD, el manifiesto semántico y los OBJ de origen no cambian hasta guardar explícitamente.','ja':'LOD{lod} から未使用のジオメトリ定義 {count} 件を削除しますか？\n\n{listing}\n\n削除予定ペイロード: ~{bytes}\n\n変更されるのはアクティブな render LOD のみです。他の LOD、semantic manifest、ソース OBJ は明示的に保存するまで変更されません。'},
'model_editor.confirm.reload_source_mesh':{
 'en':'Reload {id} directly from SOURCE?\n\n{path}\n\nOnly this mesh payload is replaced in memory. SAVE is still required.','ru':'Перезагрузить {id} напрямую из SOURCE?\n\n{path}\n\nВ памяти будет заменён только payload этого меша. После этого всё ещё требуется SAVE.','zh-Hans':'直接从 SOURCE 重新加载 {id}？\n\n{path}\n\n只会替换内存中的该网格载荷。之后仍需 SAVE。','es':'¿Recargar {id} directamente desde SOURCE?\n\n{path}\n\nSolo se reemplaza en memoria la carga de esta malla. SAVE sigue siendo necesario.','ja':'{id} を SOURCE から直接再読み込みしますか？\n\n{path}\n\nメモリ上ではこのメッシュのペイロードだけが置き換わります。引き続き SAVE が必要です。'},
'model_editor.status.source_instance_selected':{
 'en':'Selected LOD{lod} instance link: {id} → {target}','ru':'Выбрана ссылка экземпляра LOD{lod}: {id} → {target}','zh-Hans':'已选择 LOD{lod} 实例链接：{id} → {target}','es':'Enlace de instancia LOD{lod} seleccionado: {id} → {target}','ja':'LOD{lod} インスタンスリンクを選択: {id} → {target}'},
'model_editor.status.source_instance_missing':{
 'en':'INSTANCE link {id} has no resident RenderNode','ru':'У ссылки INSTANCE {id} нет загруженного RenderNode','zh-Hans':'INSTANCE 链接 {id} 没有驻留 RenderNode','es':'El enlace INSTANCE {id} no tiene RenderNode residente','ja':'INSTANCE リンク {id} には常駐 RenderNode がありません'},
'model_editor.status.source_geometry_selected':{
 'en':'Selected LOD{lod} geometry: {id}','ru':'Выбрана геометрия LOD{lod}: {id}','zh-Hans':'已选择 LOD{lod} 几何：{id}','es':'Geometría LOD{lod} seleccionada: {id}','ja':'LOD{lod} ジオメトリを選択: {id}'},
'model_editor.source.instance_badge':{
 'en':'INSTANCE','ru':'ЭКЗЕМПЛЯР','zh-Hans':'实例','es':'INSTANCIA','ja':'インスタンス'},
'model_editor.source.missing_link_kept':{
 'en':'SOURCE MISSING · LINK KEPT','ru':'SOURCE ОТСУТСТВУЕТ · СВЯЗЬ СОХРАНЕНА','zh-Hans':'SOURCE 缺失 · 保留链接','es':'SOURCE AUSENTE · ENLACE CONSERVADO','ja':'SOURCE 不明 · リンク保持'},
'model_editor.source.reload':{
 'en':'↻ SOURCE','ru':'↻ SOURCE','zh-Hans':'↻ SOURCE','es':'↻ SOURCE','ja':'↻ SOURCE'},
'model_editor.lod.source_kind':{
 'en':'source kind','ru':'тип источника','zh-Hans':'源类型','es':'tipo de origen','ja':'ソース種別'},
'model_editor.lod.placeholder_unknown':{
 'en':'SSE unknown','ru':'SSE неизвестен','zh-Hans':'SSE 未知','es':'SSE desconocido','ja':'SSE 不明'}
}
for key,row in add.items():
    S[key]=row
CAT.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding='utf-8')

# ---------------------------------------------------------------------------
# Shell/runtime localization: eliminate known direct user-facing prose.
# ---------------------------------------------------------------------------
html=HTML.read_text(encoding='utf-8')
html=html.replace('<span id="progressStage">READING</span>','<span id="progressStage" data-i18n="model_editor.busy.reading">READING</span>')
html=html.replace('<span class="sharedBadge">STATE</span>','<span class="sharedBadge" data-i18n="model_editor.common.state">STATE</span>')
html=html.replace('<option value="pivot">Selected element pivot</option>','<option value="pivot" data-i18n="model_editor.radial.center_pivot">Selected element pivot</option>')
html=html.replace('<option value="origin">Parent origin (0,0,0)</option>','<option value="origin" data-i18n="model_editor.radial.center_origin">Parent origin (0,0,0)</option>')
html=html.replace('<option value="custom">Custom</option>','<option value="custom" data-i18n="model_editor.radial.center_custom">Custom</option>')
html=html.replace('<button id="axisApplyBtn" type="button">REBUILD LOD0 + APPLY ROTATION</button>','<button id="axisApplyBtn" data-i18n="model_editor.axis_rotation.apply_button" type="button">REBUILD LOD0 + APPLY ROTATION</button>')
html=html.replace('<span id="ioActivity" class="idle">IDLE</span>','<span id="ioActivity" data-i18n="model_editor.status.idle" class="idle">IDLE</span>')
html=html.replace('<span id="ioMessage">Ready</span>','<span id="ioMessage" data-i18n="model_editor.status.ready">Ready</span>')

html=replace_once(html,"localStatus(`SURFACES: LOD${state.activeLod} · ${selected.size} mesh families selected · G${primaryGeometry.index} · ${primaryGeometry.id}`);","localStatus(tr('model_editor.status.surface_selected','SURFACES: LOD{lod} · {count} mesh families selected · G{geometry} · {id}',{lod:state.activeLod,count:selected.size,geometry:primaryGeometry.index,id:primaryGeometry.id}));",'surface selection status')
html=replace_once(html,"localStatus(`${wizardStageLabel(state.wizardStage)}: LOD${index}`);","localStatus(tr('model_editor.status.active_lod','{stage}: LOD{lod}',{stage:wizardStageLabel(state.wizardStage),lod:index}));",'active lod status')
html=html.replace("extraRoot.innerHTML='<div class=\"variantEmpty\">LOD not loaded</div>';","extraRoot.innerHTML=`<div class=\"variantEmpty\">${tr('model_editor.common.lod_not_loaded','LOD not loaded')}</div>`;")
html=html.replace("baseRoot.innerHTML='<div class=\"variantEmpty\">No used default geometry in this LOD.</div>';","baseRoot.innerHTML=`<div class=\"variantEmpty\">${tr('model_editor.geometry_variants.no_used_default','No used default geometry in this LOD.')}</div>`;")

old_stats="function geometryStatsHtml(lod){if(!lod)return'';const geos=lod.geometries||[],nodes=lod.nodes||[],totalV=geos.reduce((s,g)=>s+Number(g.vertexCount||0),0),totalT=geos.reduce((s,g)=>s+Number(g.triangleCount||0),0),bindings=nodes.filter(n=>Number(n.geometryIndex)>=0).length,shared=geos.filter(g=>!g.isSourceVariant&&Number(g.usageCount||0)>1).length,variants=geos.filter(g=>g.isSourceVariant).length,unused=unusedGeometries(activeRenderLod(state.asset?.renderLods,state.activeLod)).length;return `<div class=\"geometryStats\"><div class=\"geometryStatsTitle\">${tr('model_editor.geometry_stats.title','LOD STATISTICS')}</div><div class=\"geometryStatsGrid\"><span>render nodes</span><span class=\"value\">${nodes.length}</span><span>geometry definitions</span><span class=\"value\">${geos.length}</span><span>geometry bindings</span><span class=\"value\">${bindings}</span><span>shared geometry groups</span><span class=\"value\">${shared}</span><span>additional meshes</span><span class=\"value\">${variants}</span><span>unused ordinary geometry</span><span class=\"value\">${unused}</span><span>vertices</span><span class=\"value\">${totalV.toLocaleString()}</span><span>triangles</span><span class=\"value\">${totalT.toLocaleString()}</span></div></div>`;}"
new_stats="function geometryStatsHtml(lod){if(!lod)return'';const geos=lod.geometries||[],nodes=lod.nodes||[],totalV=geos.reduce((s,g)=>s+Number(g.vertexCount||0),0),totalT=geos.reduce((s,g)=>s+Number(g.triangleCount||0),0),bindings=nodes.filter(n=>Number(n.geometryIndex)>=0).length,shared=geos.filter(g=>!g.isSourceVariant&&Number(g.usageCount||0)>1).length,variants=geos.filter(g=>g.isSourceVariant).length,unused=unusedGeometries(activeRenderLod(state.asset?.renderLods,state.activeLod)).length;return `<div class=\"geometryStats\"><div class=\"geometryStatsTitle\">${tr('model_editor.geometry_stats.title','LOD STATISTICS')}</div><div class=\"geometryStatsGrid\"><span>${tr('model_editor.geometry_stats.render_nodes','render nodes')}</span><span class=\"value\">${nodes.length}</span><span>${tr('model_editor.geometry_stats.geometry_definitions','geometry definitions')}</span><span class=\"value\">${geos.length}</span><span>${tr('model_editor.geometry_stats.geometry_bindings','geometry bindings')}</span><span class=\"value\">${bindings}</span><span>${tr('model_editor.geometry_stats.shared_groups','shared geometry groups')}</span><span class=\"value\">${shared}</span><span>${tr('model_editor.geometry_stats.additional_meshes','additional meshes')}</span><span class=\"value\">${variants}</span><span>${tr('model_editor.geometry_stats.unused_geometry','unused ordinary geometry')}</span><span class=\"value\">${unused}</span><span>${tr('model_editor.geometry_stats.vertices','vertices')}</span><span class=\"value\">${totalV.toLocaleString()}</span><span>${tr('model_editor.geometry_stats.triangles','triangles')}</span><span class=\"value\">${totalT.toLocaleString()}</span></div></div>`;}"
html=replace_once(html,old_stats,new_stats,'geometry stats localization')

old_axis="function updateAxisLegend(){const root=$('axisLegend');if(!root)return;if(!state.asset){root.innerHTML='<span class=\"axisGame\">GAME FRAME</span>\\nRIGHT +X · UP +Y · NOSE -Z\\nGROUND XZ · VERTICAL Y';return;}const m=activeLodDirectAxisMapping(activeRenderLod(state.asset?.renderLods,state.activeLod));root.innerHTML=`<span class=\"axisGame\">GAME FRAME · FIXED</span>\\nRIGHT +X · UP +Y · NOSE -Z\\nGROUND XZ · VERTICAL Y\\n<span class=\"axisLod\">LOD${state.activeLod} SOURCE → GAME</span> · ${directAxisShort(m)}`;}"
new_axis="function updateAxisLegend(){const root=$('axisLegend');if(!root)return;const directions=tr('model_editor.axis_legend.directions','RIGHT +X · UP +Y · NOSE -Z'),ground=tr('model_editor.axis_legend.ground','GROUND XZ · VERTICAL Y');if(!state.asset){root.innerHTML=`<span class=\"axisGame\">${tr('model_editor.axis_legend.game_frame','GAME FRAME')}</span>\\n${directions}\\n${ground}`;return;}const m=activeLodDirectAxisMapping(activeRenderLod(state.asset?.renderLods,state.activeLod));root.innerHTML=`<span class=\"axisGame\">${tr('model_editor.axis_legend.game_frame_fixed','GAME FRAME · FIXED')}</span>\\n${directions}\\n${ground}\\n<span class=\"axisLod\">${tr('model_editor.axis_legend.source_to_game','LOD{lod} SOURCE → GAME',{lod:state.activeLod})}</span> · ${directAxisShort(m)}`;}"
html=replace_once(html,old_axis,new_axis,'axis legend localization')

html=html.replace("$('overlay').textContent=`semantic: ${n.id}\\npreview state: ${previewStateForNode(state.previewStates,state.asset?.nodes||[],i)}`;","$('overlay').textContent=`${tr('model_editor.overlay.semantic','semantic')}: ${n.id}\\n${tr('model_editor.overlay.preview_state','preview state')}: ${previewStateForNode(state.previewStates,state.asset?.nodes||[],i)}`;")
html=html.replace("`${Number(g.triangleCount||0).toLocaleString()} tris`","`${Number(g.triangleCount||0).toLocaleString()} ${tr('model_editor.common.tris_short','tris')}`")

# Render-active-LOD details labels.
html=html.replace('<span>source kind</span>',"<span>${tr('model_editor.lod.source_kind','source kind')}</span>")
html=html.replace('<span>render nodes</span>',"<span>${tr('model_editor.geometry_stats.render_nodes','render nodes')}</span>")
html=html.replace('<span>geometry definitions</span>',"<span>${tr('model_editor.geometry_stats.geometry_definitions','geometry definitions')}</span>")
html=html.replace('<span>geometry bindings</span>',"<span>${tr('model_editor.geometry_stats.geometry_bindings','geometry bindings')}</span>")
html=html.replace('<span>vertices</span>',"<span>${tr('model_editor.geometry_stats.vertices','vertices')}</span>")
html=html.replace('<span>triangles</span>',"<span>${tr('model_editor.geometry_stats.triangles','triangles')}</span>")
html=html.replace('placeholder="SSE unknown"','placeholder="${tr(\'model_editor.lod.placeholder_unknown\',\'SSE unknown\')}"')

# Storage block: all human-readable labels now pass through tr().
old_storage="function renderStorage(){const root=$('storage');root.innerHTML='';if(!state.asset)return;const st=state.asset.storage||{};const dirty=state.dirty?'<div class=\"warnText\" style=\"margin-top:5px\">Unsaved WORKING ASSET changes exist. SAVE keeps them; RESTORE discards them. BUILD writes production only from a saved state.</div>':'';const lods=(st.lodPayloads||[]).map(p=>`<span>WORKING LOD${p.lod} .elmesh${p.dirty?' · DIRTY':''}</span><span class=\"value\">${formatBytes(p.bytes)}</span>`).join('');const legacy=Number(st.legacyBinaryBytes)>0?`<div class=\"warnText\" style=\"margin-top:5px\">Legacy package still exists: ${formatBytes(st.legacyBinaryBytes)}<br><span class=\"storagePath\">${st.legacyBinaryPath||''}</span></div>`:'';root.innerHTML=`<div class=\"storageGrid\"><span>source meshes</span><span class=\"value\">${formatBytes(st.sourceMeshBytes)}</span><span>WORKING manifest${st.manifestDirty?' · DIRTY':''}</span><span class=\"value\">${formatBytes(st.workingManifestBytes)}</span>${lods}<span>WORKING package total</span><span class=\"value\">${formatBytes(st.workingPackageBytes)}</span><span>PRODUCTION package (last BUILD)</span><span class=\"value\">${formatBytes(st.productionPackageBytes)}</span><span>current render payload estimate</span><span class=\"value\">~${formatBytes(st.estimatedGeometryPayloadBytes)}</span><span>unused active-LOD geometry</span><span class=\"value\">${activeRenderLod(state.asset?.renderLods,state.activeLod)?(activeRenderLod(state.asset?.renderLods,state.activeLod).geometries||[]).filter(g=>!(Number(g.usageCount)>0)&&!g.isSourceVariant).length:0}</span><span>source variant geometries</span><span class=\"value\">${Number(st.sourceVariantGeometryCount||0)}</span></div><div class=\"storagePath\" style=\"margin-top:6px\">WORKING: ${st.workingBinaryPath||state.asset.workingAssetPath||''}</div><div class=\"storagePath\">PRODUCTION: ${st.binaryPath||state.asset.binaryPath||''}</div><div class=\"help\" style=\"margin-top:5px\">WORKING ASSET is the saved editor resume state. SAVE and RESTORE are the only working-state persistence controls. Production changes only at BUILD. Source OBJ/MTL files are read-only authoring inputs.</div>${legacy}${dirty}`;}"
new_storage="function renderStorage(){const root=$('storage');root.innerHTML='';if(!state.asset)return;const st=state.asset.storage||{};const dirty=state.dirty?`<div class=\"warnText\" style=\"margin-top:5px\">${tr('model_editor.storage.unsaved_warning','Unsaved WORKING ASSET changes exist. SAVE keeps them; RESTORE discards them. BUILD writes production only from a saved state.')}</div>`:'';const lods=(st.lodPayloads||[]).map(p=>`<span>${tr('model_editor.storage.working_lod','WORKING LOD{lod} .elmesh',{lod:p.lod})}${p.dirty?' · DIRTY':''}</span><span class=\"value\">${formatBytes(p.bytes)}</span>`).join('');const legacy=Number(st.legacyBinaryBytes)>0?`<div class=\"warnText\" style=\"margin-top:5px\">${tr('model_editor.storage.legacy_package','Legacy package still exists')}: ${formatBytes(st.legacyBinaryBytes)}<br><span class=\"storagePath\">${st.legacyBinaryPath||''}</span></div>`:'';root.innerHTML=`<div class=\"storageGrid\"><span>${tr('model_editor.storage.source_meshes','source meshes')}</span><span class=\"value\">${formatBytes(st.sourceMeshBytes)}</span><span>${tr('model_editor.storage.working_manifest','WORKING manifest')}${st.manifestDirty?' · DIRTY':''}</span><span class=\"value\">${formatBytes(st.workingManifestBytes)}</span>${lods}<span>${tr('model_editor.storage.working_total','WORKING package total')}</span><span class=\"value\">${formatBytes(st.workingPackageBytes)}</span><span>${tr('model_editor.storage.production_last_build','PRODUCTION package (last BUILD)')}</span><span class=\"value\">${formatBytes(st.productionPackageBytes)}</span><span>${tr('model_editor.storage.render_payload_estimate','current render payload estimate')}</span><span class=\"value\">~${formatBytes(st.estimatedGeometryPayloadBytes)}</span><span>${tr('model_editor.storage.unused_active_lod','unused active-LOD geometry')}</span><span class=\"value\">${activeRenderLod(state.asset?.renderLods,state.activeLod)?(activeRenderLod(state.asset?.renderLods,state.activeLod).geometries||[]).filter(g=>!(Number(g.usageCount)>0)&&!g.isSourceVariant).length:0}</span><span>${tr('model_editor.storage.source_variants','source variant geometries')}</span><span class=\"value\">${Number(st.sourceVariantGeometryCount||0)}</span></div><div class=\"storagePath\" style=\"margin-top:6px\">${tr('model_editor.storage.working_path','WORKING')}: ${st.workingBinaryPath||state.asset.workingAssetPath||''}</div><div class=\"storagePath\">${tr('model_editor.storage.production_path','PRODUCTION')}: ${st.binaryPath||state.asset.binaryPath||''}</div><div class=\"help\" style=\"margin-top:5px\">${tr('model_editor.storage.help','WORKING ASSET is the saved editor resume state. SAVE and RESTORE are the only working-state persistence controls. Production changes only at BUILD. Source OBJ/MTL files are read-only authoring inputs.')}</div>${legacy}${dirty}`;}"
html=replace_once(html,old_storage,new_storage,'storage localization')

old_confirm="if(!confirm(`Delete ${unused.length} unused geometry definition(s) from LOD${state.activeLod}?\\n\\n${listing}\\n\\nEstimated payload removed: ~${formatBytes(bytes)}\\n\\nOnly the active render LOD changes. Other LODs, the semantic manifest and source OBJ files are unchanged until explicitly saved.`))"
new_confirm="if(!confirm(tr('model_editor.confirm.delete_unused_geometry','Delete {count} unused geometry definition(s) from LOD{lod}?\\n\\n{listing}\\n\\nEstimated payload removed: ~{bytes}\\n\\nOnly the active render LOD changes. Other LODs, the semantic manifest and source OBJ files are unchanged until explicitly saved.',{count:unused.length,lod:state.activeLod,listing,bytes:formatBytes(bytes)})))"
html=replace_once(html,old_confirm,new_confirm,'unused geometry confirm')

# SOURCE inventory direct statuses / confirmation.
html=html.replace("localStatus(`Selected LOD${state.activeLod} instance link: ${g.id} → ${g.instanceOfGeometryId}`)","localStatus(tr('model_editor.status.source_instance_selected','Selected LOD{lod} instance link: {id} → {target}',{lod:state.activeLod,id:g.id,target:g.instanceOfGeometryId}))")
html=html.replace("localStatus(`INSTANCE link ${g.id} has no resident RenderNode`,'danger')","localStatus(tr('model_editor.status.source_instance_missing','INSTANCE link {id} has no resident RenderNode',{id:g.id}),'danger')")
html=html.replace("localStatus(`Selected LOD${state.activeLod} geometry: ${g.id}`)","localStatus(tr('model_editor.status.source_geometry_selected','Selected LOD{lod} geometry: {id}',{lod:state.activeLod,id:g.id}))")
html=html.replace("confirm(`Reload ${g.id} directly from SOURCE?\\n\\n${g.sourcePath}\\n\\nOnly this mesh payload is replaced in memory. SAVE is still required.`)","confirm(tr('model_editor.confirm.reload_source_mesh','Reload {id} directly from SOURCE?\\n\\n{path}\\n\\nOnly this mesh payload is replaced in memory. SAVE is still required.',{id:g.id,path:g.sourcePath}))")
html=html.replace("document.createTextNode('INSTANCE')","document.createTextNode(tr('model_editor.source.instance_badge','INSTANCE'))")
html=html.replace("document.createTextNode('SOURCE MISSING · LINK KEPT')","document.createTextNode(tr('model_editor.source.missing_link_kept','SOURCE MISSING · LINK KEPT'))")
html=html.replace("document.createTextNode('↻ SOURCE')","document.createTextNode(tr('model_editor.source.reload','↻ SOURCE'))")

HTML.write_text(html,encoding='utf-8')

# Document title belongs to the same catalog and changes immediately with locale.
p=WEB/'effects/i18n.js'; text=p.read_text(encoding='utf-8')
text=replace_once(text,"function applyLocale(locale=state.locale){state.locale=state.localeOrder.includes(locale)?locale:(state.i18n?.default_locale||'en');document.documentElement.lang=state.locale;applyDocumentTranslations(document);", "function applyLocale(locale=state.locale){state.locale=state.localeOrder.includes(locale)?locale:(state.i18n?.default_locale||'en');document.documentElement.lang=state.locale;document.title=tr('model_editor.document.title','Elite Model Asset Editor');applyDocumentTranslations(document);",'localized document title')
p.write_text(text,encoding='utf-8')

# ---------------------------------------------------------------------------
# Stronger localization contract: real translation coverage and no regression
# to known bypasses in the high-visibility shell.
# ---------------------------------------------------------------------------
p=TESTS/'check_model_asset_editor_localization.py'; text=p.read_text(encoding='utf-8')
text=text.replace("if 'ModelAssetEditorVersion = \"0.10.82\"' not in version: errors.append('expected editor version 0.10.82')","if 'ModelAssetEditorVersion = \"0.10.83\"' not in version: errors.append('expected editor version 0.10.83')")
insert="""
# High-visibility shell coverage. Technical identifiers (LOD0, XYZ, file names,
# numeric badges) may remain literal; human prose must be keyed or call tr().
for required in [
 'data-i18n=\"model_editor.busy.reading\"',
 'data-i18n=\"model_editor.common.state\"',
 'data-i18n=\"model_editor.radial.center_pivot\"',
 'data-i18n=\"model_editor.radial.center_origin\"',
 'data-i18n=\"model_editor.radial.center_custom\"',
 'data-i18n=\"model_editor.axis_rotation.apply_button\"',
 'data-i18n=\"model_editor.status.idle\"',
 'data-i18n=\"model_editor.status.ready\"'
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
 (r"confirm\(\s*`Delete \\$\{unused\.length\}",'unused geometry confirmation'),
 (r"confirm\(\s*`Reload \\$\{g\.id\} directly from SOURCE",'source reload confirmation'),
 (r"<span>source meshes</span>",'storage labels'),
 (r"WORKING ASSET is the saved editor resume state",'storage help'),
 (r"GAME FRAME · FIXED</span>",'axis legend')
]:
 if re.search(pattern,html): errors.append(f'direct localization bypass remains: {label}')
# Fallback English is permitted only when a locale value is actually absent.
# A present non-English locale may not silently copy full English prose.
"""
marker="version=VERSION.read_text(encoding='utf-8')\n"
text=text.replace(marker,insert+marker)
p.write_text(text,encoding='utf-8')

# Bump all version-coupled MAE architecture contracts.
for path in TESTS.glob('check_model_asset_*.py'):
    t=path.read_text(encoding='utf-8').replace(r'0\.10\.82',r'0\.10\.83').replace('0.10.82','0.10.83')
    path.write_text(t,encoding='utf-8')
VERSION.write_text(VERSION.read_text(encoding='utf-8').replace('0.10.82','0.10.83'),encoding='utf-8')

STATE.write_text("""# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.83
**WebUI architectural layer separation:** ~100%
**Localization architecture:** closed candidate

## Accepted baseline

The user locally verified v0.10.81 architecture contracts, CMake build and runtime startup/basic workflow. Application state, orchestration, transport, backend session/persistence, EditorViewState and THREE viewport responsibilities are physically separated behind explicit module boundaries.

## v0.10.83 localization completion

Localization is a first-class architectural boundary:

- one five-locale catalog is authoritative;
- `i18n/catalog.js` is the PURE resolver/formatter;
- `i18n/dom.js` handles declarative static DOM translation;
- `effects/i18n.js` owns locale orchestration and document-title refresh;
- a language selected before asset/settings arrival remains authoritative and persists later;
- static shell, contextual help, SEMANTICS workflow, status bar, geometry statistics, axis legend and storage/read-model labels resolve through localization keys;
- backend protocol supports stable `messageKey` + `messageParams`; legacy/raw diagnostic messages are allowed to fall back to English exactly as the product fallback policy requires;
- the localization contract rejects full English-prose copies inside populated non-English locale values and rejects known direct high-visibility UI bypasses.

English fallback is intentional only when a translation is genuinely absent. A populated locale entry that merely copies English prose is treated as a localization defect.

## Testing strategy

`run_model_asset_editor_impacted.py --base HEAD^` runs contracts selected by changed paths during normal iteration. The full architecture gate remains mandatory for closure/release.

## Next architecture task

After localization acceptance: **finish binary v4 translation-unit architecture** — list binary `.cpp` files directly in the CMake target, remove facade `.cpp` aggregation includes, enforce the boundary contract, and build/test v4 save/load.
""",encoding='utf-8')

TASK.write_text("""# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.83
**WebUI architecture:** ~100%

## Immediate goal

Acceptance-test localization completeness across all five locales. Verify switching before any asset is selected, permanent `Ctrl+Alt+F12` hint, settings persistence, static shell, contextual `?` help, SOURCE/LODS/GEOMETRY/SURFACES/SEMANTICS/PHYSICS/DAMAGE, axis legend, storage panel and representative backend status messages.

English fallback is valid only when the requested locale has no translation. A non-English catalog field populated with copied English prose is a defect.

## Normal iteration gate

```bash
python tests/architecture_contracts/run_model_asset_editor_impacted.py --base HEAD^
```

## Full gate for v0.10.83

```bash
python tests/architecture_contracts/check_model_asset_editor_application_state.py
python tests/architecture_contracts/check_model_asset_editor_orchestration_layers.py
python tests/architecture_contracts/check_model_asset_editor_transport_layers.py
python tests/architecture_contracts/check_model_asset_editor_session_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_layers.py
python tests/architecture_contracts/check_model_asset_editor_viewport_adapters.py
python tests/architecture_contracts/check_model_asset_editor_view_state_layers.py
python tests/architecture_contracts/check_model_asset_editor_shell_architecture.py
python tests/architecture_contracts/check_model_asset_editor_localization.py
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

## NEXT TASK AFTER LOCALIZATION

**Finish binary v4 translation-unit architecture.**

Required closure:

1. list the binary subsystem `.cpp` files directly in the `EliteModelAsset` CMake target;
2. remove `.cpp` aggregation includes from the facade;
3. add/strengthen the architecture contract so `.cpp` includes cannot return;
4. build and test production v4 save/load compatibility.
""",encoding='utf-8')

print('MAE v0.10.83 localization completeness pass staged')
