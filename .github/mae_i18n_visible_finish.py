from pathlib import Path
import json,re

ROOT=Path(__file__).resolve().parents[1]
HTML=ROOT/'src/assets/webui/model_asset_editor.html'
WEB=ROOT/'src/assets/webui/model_asset_editor'
CAT=ROOT/'src/assets/localization/ui/tools/model_asset_editor.json'
TEST=ROOT/'tests/architecture_contracts/check_model_asset_editor_localization.py'


def rep(text,old,new,label,required=True):
    n=text.count(old)
    if required and n<1: raise RuntimeError(f'{label}: no match')
    return text.replace(old,new)

D=json.loads(CAT.read_text(encoding='utf-8')); S=D['strings']

def put(key,en,ru,zh,es,ja):
    S[key]={'en':en,'ru':ru,'zh-Hans':zh,'es':es,'ja':ja}

# Shared visible chrome / technical UI labels. Product identifiers such as LOD,
# SOURCE, WORKING, RenderNode and file extensions intentionally remain terms.
put('model_editor.common.selected','SELECTED','ВЫБРАНО','已选择','SELECCIONADO','選択中')
put('model_editor.common.main','MAIN','ОСНОВНОЙ','主网格','PRINCIPAL','メイン')
put('model_editor.common.replacement','REPLACEMENT','ЗАМЕНА','替换','REEMPLAZO','置換')
put('model_editor.common.authored','AUTHORED','АВТОРСКИЙ','已制作','AUTORÍA','作成済み')
put('model_editor.common.view','VIEW','ПОКАЗАТЬ','查看','VER','表示')
put('model_editor.common.load','LOAD','ЗАГРУЗИТЬ','加载','CARGAR','読み込み')
put('model_editor.common.use','USE','ИСПОЛЬЗОВАТЬ','使用','USAR','使用')
put('model_editor.common.primary','PRIMARY','ОСНОВНОЙ','主项','PRINCIPAL','プライマリ')
put('model_editor.common.meshes_count','{count} MESHES','МЕШЕЙ: {count}','{count} 个网格','{count} MALLAS','{count} メッシュ')
put('model_editor.common.apply_selected','APPLY TO {count} SELECTED','ПРИМЕНИТЬ К ВЫБРАННЫМ: {count}','应用到 {count} 个已选项','APLICAR A {count} SELECCIONADOS','選択した {count} 件に適用')
put('model_editor.common.primary_mesh_only','PRIMARY MESH ONLY','ТОЛЬКО ОСНОВНОЙ МЕШ','仅主网格','SOLO MALLA PRINCIPAL','プライマリメッシュのみ')
put('model_editor.common.detached','DETACHED','ОТДЕЛЁН','已分离','SEPARADO','分離済み')
put('model_editor.common.pass','PASS','ПРОЙДЕНО','通过','CORRECTO','合格')
put('model_editor.common.fail','FAIL','ОШИБКА','失败','FALLO','失敗')
put('model_editor.common.hide_item','Hide {id}','Скрыть {id}','隐藏 {id}','Ocultar {id}','{id} を非表示')
put('model_editor.common.show_item','Show {id}','Показать {id}','显示 {id}','Mostrar {id}','{id} を表示')

put('model_editor.geometry_inventory.view_lod','VIEW LOD{lod}','ПОКАЗАТЬ LOD{lod}','查看 LOD{lod}','VER LOD{lod}','LOD{lod} を表示')
put('model_editor.geometry_inventory.load_lod','LOAD LOD{lod}','ЗАГРУЗИТЬ LOD{lod}','加载 LOD{lod}','CARGAR LOD{lod}','LOD{lod} を読み込み')
put('model_editor.geometry_inventory.all','● ALL','● ВСЕ','● 全部','● TODO','● すべて')
put('model_editor.geometry_inventory.none','○ NONE','○ НИЧЕГО','○ 全不显示','○ NINGUNO','○ なし')
put('model_editor.geometry_inventory.visible','{visible}/{total} VISIBLE','ВИДИМО {visible}/{total}','可见 {visible}/{total}','VISIBLES {visible}/{total}','表示 {visible}/{total}')
put('model_editor.geometry_inventory.isolate_tip','Click to isolate this mesh family','Нажмите, чтобы изолировать это семейство мешей','点击以隔离此网格族','Haz clic para aislar esta familia de mallas','クリックしてこのメッシュ系統を分離表示')
put('model_editor.geometry_inventory.hide_tip','Hide this mesh family','Скрыть это семейство мешей','隐藏此网格族','Ocultar esta familia de mallas','このメッシュ系統を非表示')
put('model_editor.geometry_inventory.show_tip','Show this mesh family','Показать это семейство мешей','显示此网格族','Mostrar esta familia de mallas','このメッシュ系統を表示')
put('model_editor.geometry_inventory.shared_tip','Shared geometry properties come from {id}. SOURCE provenance remains {source}.','Свойства общей геометрии берутся из {id}. Происхождение SOURCE остаётся {source}.','共享几何属性来自 {id}。SOURCE 来源仍为 {source}。','Las propiedades de geometría compartida proceden de {id}. La procedencia SOURCE sigue siendo {source}.','共有ジオメトリのプロパティは {id} から取得します。SOURCE の由来は {source} のままです。')
put('model_editor.geometry_inventory.reload_tip','Reload only this mesh from linked SOURCE OBJ. This is not RELOAD LOD.','Перезагрузить только этот меш из связанного SOURCE OBJ. Это не RELOAD LOD.','仅从已链接的 SOURCE OBJ 重新加载此网格。这不是 RELOAD LOD。','Recarga solo esta malla desde el OBJ SOURCE enlazado. No es RELOAD LOD.','リンクされた SOURCE OBJ からこのメッシュだけを再読み込みします。RELOAD LOD ではありません。')
put('model_editor.lod.reload_native_tip','RELOAD LOD{lod} from saved WORKING/PRODUCTION .elmesh. Never reads SOURCE OBJ.','RELOAD LOD{lod} из сохранённого WORKING/PRODUCTION .elmesh. SOURCE OBJ не читается.','从已保存的 WORKING/PRODUCTION .elmesh 重新加载 LOD{lod}。不会读取 SOURCE OBJ。','RELOAD LOD{lod} desde el .elmesh guardado de WORKING/PRODUCTION. Nunca lee el OBJ SOURCE.','保存済み WORKING/PRODUCTION .elmesh から LOD{lod} を再読み込みします。SOURCE OBJ は読みません。')
put('model_editor.overlay.render_detail','render LOD{lod}: {id}\ngeometry: {geometry}\nsemantic: {semantic}','рендер LOD{lod}: {id}\nгеометрия: {geometry}\nсемантика: {semantic}','渲染 LOD{lod}：{id}\n几何：{geometry}\n语义：{semantic}','render LOD{lod}: {id}\ngeometría: {geometry}\nsemántica: {semantic}','レンダー LOD{lod}: {id}\nジオメトリ: {geometry}\nセマンティック: {semantic}')
put('model_editor.geometry_compare.instance_link','INSTANCE LINK · {from} → {to}','СВЯЗЬ ЭКЗЕМПЛЯРА · {from} → {to}','实例链接 · {from} → {to}','ENLACE DE INSTANCIA · {from} → {to}','インスタンスリンク · {from} → {to}')
put('model_editor.geometry_compare.isolate_tip','Click to isolate this mesh','Нажмите, чтобы изолировать этот меш','点击以隔离此网格','Haz clic para aislar esta malla','クリックしてこのメッシュを分離表示')
put('model_editor.geometry_compare.hide_tip','Hide this mesh','Скрыть этот меш','隐藏此网格','Ocultar esta malla','このメッシュを非表示')
put('model_editor.geometry_compare.show_tip','Show this mesh','Показать этот меш','显示此网格','Mostrar esta malla','このメッシュを表示')

put('model_editor.maintenance.results','{count} RESULT(S)','РЕЗУЛЬТАТОВ: {count}','{count} 个结果','{count} RESULTADO(S)','結果 {count} 件')
put('model_editor.maintenance.source_current','SOURCE CURRENT','SOURCE АКТУАЛЕН','SOURCE 已是最新','SOURCE ACTUALIZADO','SOURCE は最新')
put('model_editor.maintenance.source_current_detail','SOURCE CURRENT · all stored file hashes match','SOURCE АКТУАЛЕН · все сохранённые хеши файлов совпадают','SOURCE 已是最新 · 所有保存的文件哈希均匹配','SOURCE ACTUALIZADO · coinciden todos los hashes guardados','SOURCE は最新 · 保存済みファイルハッシュはすべて一致')
put('model_editor.maintenance.scan_metrics','folder: {folder} · {hashes} hashes · {walks} directory walks · {ms} ms · unchanged {unchanged} / replaced {replaced} / added {added} / missing {missing} / failed {failed}','папка: {folder} · хешей: {hashes} · обходов папок: {walks} · {ms} мс · без изменений {unchanged} / заменено {replaced} / добавлено {added} / отсутствует {missing} / ошибок {failed}','文件夹：{folder} · 哈希读取 {hashes} · 目录遍历 {walks} · {ms} ms · 未更改 {unchanged} / 已替换 {replaced} / 已添加 {added} / 缺失 {missing} / 失败 {failed}','carpeta: {folder} · {hashes} hashes · {walks} recorridos · {ms} ms · sin cambios {unchanged} / reemplazados {replaced} / añadidos {added} / ausentes {missing} / fallidos {failed}','フォルダ: {folder} · ハッシュ {hashes} · ディレクトリ走査 {walks} · {ms} ms · 未変更 {unchanged} / 置換 {replaced} / 追加 {added} / 不明 {missing} / 失敗 {failed}')
put('model_editor.source.missing_instance_link','SOURCE FILE MISSING · INSTANCE LINK KEPT','SOURCE-ФАЙЛ ОТСУТСТВУЕТ · СВЯЗЬ ЭКЗЕМПЛЯРА СОХРАНЕНА','SOURCE 文件缺失 · 保留实例链接','FALTA ARCHIVO SOURCE · SE CONSERVA EL ENLACE DE INSTANCIA','SOURCE ファイル不明 · インスタンスリンク保持')

put('model_editor.surfaces.mixed','— MIXED —','— СМЕШАННО —','— 混合 —','— MIXTO —','— 混在 —')
put('model_editor.surfaces.used_here_tag','USED HERE','ИСПОЛЬЗУЕТСЯ ЗДЕСЬ','此处使用','USADO AQUÍ','ここで使用')
put('model_editor.surfaces.active_meshes','ACTIVE LOD MESHES / {geometry}','МЕШИ АКТИВНОГО LOD / {geometry}','活动 LOD 网格 / {geometry}','MALLAS DEL LOD ACTIVO / {geometry}','アクティブ LOD メッシュ / {geometry}')
put('model_editor.surfaces.visible','{visible}/{total} VISIBLE','ВИДИМО {visible}/{total}','可见 {visible}/{total}','VISIBLES {visible}/{total}','表示 {visible}/{total}')
put('model_editor.surfaces.selection_help','Click = one mesh · Ctrl = add/remove one · Shift = select range. Surface type applies to the whole selected group; material editing stays on the primary row.','Клик = один меш · Ctrl = добавить/убрать один · Shift = выбрать диапазон. Тип поверхности применяется ко всей выбранной группе; материал редактируется только для основной строки.','单击 = 一个网格 · Ctrl = 添加/移除一个 · Shift = 选择范围。表面类型应用于整个已选组；材质编辑只作用于主行。','Clic = una malla · Ctrl = añadir/quitar una · Shift = seleccionar rango. El tipo de superficie se aplica a todo el grupo; el material se edita solo en la fila principal.','クリック = 1 メッシュ · Ctrl = 1 件追加/解除 · Shift = 範囲選択。サーフェスタイプは選択グループ全体に適用し、マテリアル編集はプライマリ行だけに適用します。')
put('model_editor.surfaces.surface_check','SURFACES CHECK: {status}','ПРОВЕРКА ПОВЕРХНОСТЕЙ: {status}','表面检查：{status}','COMPROBACIÓN DE SUPERFICIES: {status}','サーフェスチェック: {status}')

# Physical-size authoring block.
put('model_editor.physical_scale.title','PHYSICAL SCALE · AUTHORING → METERS','ФИЗИЧЕСКИЙ МАСШТАБ · AUTHORING → МЕТРЫ','物理比例 · 制作空间 → 米','ESCALA FÍSICA · AUTORÍA → METROS','物理スケール · オーサリング → メートル')
put('model_editor.physical_scale.help','SOURCE and WORKING are never resized. Set one asset-wide coefficient manually; BUILD applies it once to a temporary production copy for renderer/physics.','SOURCE и WORKING никогда не масштабируются. Задайте один коэффициент для всего asset; BUILD применит его один раз к временной production-копии для renderer/physics.','SOURCE 和 WORKING 从不缩放。手动设置一个全资源系数；BUILD 仅对用于渲染/物理的临时 production 副本应用一次。','SOURCE y WORKING nunca se redimensionan. Define un coeficiente para todo el recurso; BUILD lo aplica una vez a una copia temporal de production para render/física.','SOURCE と WORKING は拡縮しません。アセット全体の係数を 1 つ設定し、BUILD が renderer/physics 用の一時 production コピーに一度だけ適用します。')
put('model_editor.physical_scale.authoring_size','AUTHORING SIZE · RAW SOURCE SPACE','РАЗМЕР AUTHORING · ИСХОДНОЕ ПРОСТРАНСТВО SOURCE','制作尺寸 · 原始 SOURCE 空间','TAMAÑO DE AUTORÍA · ESPACIO SOURCE ORIGINAL','オーサリングサイズ · RAW SOURCE 空間')
put('model_editor.physical_scale.game_size','GAME SIZE','РАЗМЕР В ИГРЕ','游戏尺寸','TAMAÑO EN JUEGO','ゲームサイズ')
put('model_editor.physical_scale.game_width','GAME WIDTH · X','ШИРИНА В ИГРЕ · X','游戏宽度 · X','ANCHO EN JUEGO · X','ゲーム幅 · X')
put('model_editor.physical_scale.game_height','GAME HEIGHT · Y','ВЫСОТА В ИГРЕ · Y','游戏高度 · Y','ALTURA EN JUEGO · Y','ゲーム高さ · Y')
put('model_editor.physical_scale.game_length','GAME LENGTH · Z','ДЛИНА В ИГРЕ · Z','游戏长度 · Z','LONGITUD EN JUEGO · Z','ゲーム長 · Z')
put('model_editor.physical_scale.game_not_linked','GAME LINK · NOT LINKED','СВЯЗЬ С ИГРОЙ · НЕ ЗАДАНА','游戏链接 · 未连接','ENLACE CON JUEGO · NO VINCULADO','ゲームリンク · 未接続')
put('model_editor.physical_scale.physical_x','PHYSICAL X','ФИЗИЧЕСКИЙ X','物理 X','X FÍSICO','物理 X')
put('model_editor.physical_scale.physical_y','PHYSICAL Y','ФИЗИЧЕСКИЙ Y','物理 Y','Y FÍSICO','物理 Y')
put('model_editor.physical_scale.physical_z','PHYSICAL Z','ФИЗИЧЕСКИЙ Z','物理 Z','Z FÍSICO','物理 Z')
put('model_editor.physical_scale.source_to_meters','SOURCE → METERS','SOURCE → МЕТРЫ','SOURCE → 米','SOURCE → METROS','SOURCE → メートル')
put('model_editor.physical_scale.calibration','CALIBRATION','КАЛИБРОВКА','校准','CALIBRACIÓN','キャリブレーション')
put('model_editor.physical_scale.not_calibrated','PHYSICAL SCALE · NOT CALIBRATED. WORKING geometry remains raw SOURCE coordinates.','ФИЗИЧЕСКИЙ МАСШТАБ · НЕ КАЛИБРОВАН. Геометрия WORKING остаётся в исходных координатах SOURCE.','物理比例 · 未校准。WORKING 几何保持原始 SOURCE 坐标。','ESCALA FÍSICA · SIN CALIBRAR. La geometría WORKING permanece en coordenadas SOURCE originales.','物理スケール · 未校正。WORKING ジオメトリは RAW SOURCE 座標のままです。')
put('model_editor.physical_scale.legacy_warning','LEGACY DESTRUCTIVE SCALE STATE · incremental SOURCE scan/reload is blocked. Use RELOAD ALL SOURCE MESHES once to migrate back to raw authoring space, then SAVE.','УСТАРЕВШЕЕ ДЕСТРУКТИВНОЕ СОСТОЯНИЕ МАСШТАБА · инкрементальные SOURCE scan/reload заблокированы. Один раз выполните RELOAD ALL SOURCE MESHES, чтобы вернуться в исходное authoring-пространство, затем SAVE.','旧版破坏性缩放状态 · 增量 SOURCE 扫描/重载被阻止。执行一次 RELOAD ALL SOURCE MESHES 以迁回原始制作空间，然后 SAVE。','ESTADO DE ESCALA DESTRUCTIVO HEREDADO · el escaneo/recarga incremental de SOURCE está bloqueado. Ejecuta RELOAD ALL SOURCE MESHES una vez para volver al espacio de autoría original y luego SAVE.','旧破壊的スケール状態 · SOURCE の増分 scan/reload は無効です。RELOAD ALL SOURCE MESHES を一度実行して RAW オーサリング空間へ戻し、その後 SAVE してください。')
put('model_editor.physical_scale.reference_axis','REFERENCE AXIS','ОПОРНАЯ ОСЬ','参考轴','EJE DE REFERENCIA','基準軸')
put('model_editor.physical_scale.extent','PHYSICAL EXTENT','ФИЗИЧЕСКИЙ РАЗМЕР','物理尺寸','EXTENSIÓN FÍSICA','物理寸法')
put('model_editor.physical_scale.set_contract','↔ SET SCALE CONTRACT','↔ ЗАДАТЬ КОНТРАКТ МАСШТАБА','↔ 设置比例契约','↔ DEFINIR CONTRATO DE ESCALA','↔ スケール契約を設定')

put('model_editor.lod.preflight.selected','SELECTED · LOD{lod}/RN{rn}{geometry} · {id}','ВЫБРАНО · LOD{lod}/RN{rn}{geometry} · {id}','已选择 · LOD{lod}/RN{rn}{geometry} · {id}','SELECCIONADO · LOD{lod}/RN{rn}{geometry} · {id}','選択中 · LOD{lod}/RN{rn}{geometry} · {id}')
put('model_editor.lod.preflight.instance_suffix',' · INSTANCE {from} → {to}',' · ЭКЗЕМПЛЯР {from} → {to}',' · 实例 {from} → {to}',' · INSTANCIA {from} → {to}',' · インスタンス {from} → {to}')
put('model_editor.lod.preflight.manual_flip','MANUAL FLIP','РУЧНОЙ FLIP','手动翻转','VOLTEO MANUAL','手動反転')
put('model_editor.lod.preflight.manual_flip_stale','MANUAL FLIP · STALE','РУЧНОЙ FLIP · УСТАРЕЛ','手动翻转 · 已过期','VOLTEO MANUAL · OBSOLETO','手動反転 · 古い')
put('model_editor.lod_generator.view_title','VIEW','ПОКАЗАТЬ','查看','VER','表示')
put('model_editor.lod_generator.lod0_authoritative','LOD0 is authoritative','LOD0 является эталонным','LOD0 为权威版本','LOD0 es autoritativo','LOD0 が基準です')
put('model_editor.lod_generator.authored','AUTHORED','АВТОРСКИЙ','已制作','AUTORÍA','作成済み')
put('model_editor.lod_generator.header_view','VIEW','ВИД','查看','VER','表示')
put('model_editor.lod_generator.header_use','USE','ИСП.','使用','USAR','使用')
put('model_editor.lod_generator.mesh_label','{kind} · G{geometry} · {id} · {triangles} T','{kind} · G{geometry} · {id} · {triangles} T','{kind} · G{geometry} · {id} · {triangles} T','{kind} · G{geometry} · {id} · {triangles} T','{kind} · G{geometry} · {id} · {triangles} T')

put('model_editor.save_stamp.title','Loaded save revision: {revision}\nSaved: {saved}\nAuthority on OPEN: {authority}{source}','Загруженная ревизия сохранения: {revision}\nСохранено: {saved}\nИсточник при OPEN: {authority}{source}','已加载保存版本：{revision}\n保存时间：{saved}\nOPEN 时的权威来源：{authority}{source}','Revisión cargada: {revision}\nGuardado: {saved}\nAutoridad al ABRIR: {authority}{source}','読み込んだ保存リビジョン: {revision}\n保存: {saved}\nOPEN 時の権威: {authority}{source}')
put('model_editor.save_stamp.source_suffix','\nSOURCE folder: {source}','\nпапка SOURCE: {source}','\nSOURCE 文件夹：{source}','\ncarpeta SOURCE: {source}','\nSOURCE フォルダ: {source}')
put('model_editor.save_stamp.legacy_unsaved','legacy/unsaved','устаревшее/не сохранено','旧版/未保存','heredado/sin guardar','旧形式/未保存')

CAT.write_text(json.dumps(D,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

# ----------------------------- HTML shell -----------------------------
h=HTML.read_text(encoding='utf-8')
h=h.replace("status=`INSTANCE LINK · ${alias.geometryId} → ${alias.instanceOfGeometryId}`","status=tr('model_editor.geometry_compare.instance_link','INSTANCE LINK · {from} → {to}',{from:alias.geometryId,to:alias.instanceOfGeometryId})")
h=h.replace("geometryStageVisibilitySet(state.activeLod,false)===null?'Click to isolate this mesh':visible?'Hide this mesh':'Show this mesh'","geometryStageVisibilitySet(state.activeLod,false)===null?tr('model_editor.geometry_compare.isolate_tip','Click to isolate this mesh'):visible?tr('model_editor.geometry_compare.hide_tip','Hide this mesh'):tr('model_editor.geometry_compare.show_tip','Show this mesh')")
h=h.replace("aria-label=\"show ${escapeMarkup(item.node.id)}\"","aria-label=\"${escapeMarkup(tr('model_editor.common.show_item','Show {id}',{id:item.node.id}))}\"")

h=h.replace("rows.length?rows.length+' RESULT(S)':'SOURCE CURRENT'","rows.length?tr('model_editor.maintenance.results','{count} RESULT(S)',{count:rows.length}):tr('model_editor.maintenance.source_current','SOURCE CURRENT')")
old="`<div class=\"sectionHint\">folder: ${escapeMarkup(scan.sourceAssetDirectory||state.asset?.sourceAssetDirectory||'—')} · ${Number(scan.hashReads||0)} hashes · ${Number(scan.directoryEnumerations||0)} directory walks · ${Number(scan.elapsedMs||0).toFixed(1)} ms · unchanged ${Number(scan.unchanged||0)} / replaced ${Number(scan.replaced||0)} / added ${Number(scan.new||0)} / missing ${Number(scan.missingSource||0)} / failed ${Number(scan.failed||0)}</div>`"
new="`<div class=\"sectionHint\">${escapeMarkup(tr('model_editor.maintenance.scan_metrics','folder: {folder} · {hashes} hashes · {walks} directory walks · {ms} ms · unchanged {unchanged} / replaced {replaced} / added {added} / missing {missing} / failed {failed}',{folder:scan.sourceAssetDirectory||state.asset?.sourceAssetDirectory||'—',hashes:Number(scan.hashReads||0),walks:Number(scan.directoryEnumerations||0),ms:Number(scan.elapsedMs||0).toFixed(1),unchanged:Number(scan.unchanged||0),replaced:Number(scan.replaced||0),added:Number(scan.new||0),missing:Number(scan.missingSource||0),failed:Number(scan.failed||0)}))}</div>`"
h=rep(h,old,new,'maintenance metrics')
h=h.replace("alias?'SOURCE FILE MISSING · INSTANCE LINK KEPT':tr('model_editor.source.deleted','DELETED')","alias?tr('model_editor.source.missing_instance_link','SOURCE FILE MISSING · INSTANCE LINK KEPT'):tr('model_editor.source.deleted','DELETED')")
h=h.replace("`<span class=\"badge ok\">INSTANCE → ${escapeMarkup(r.instanceOfGeometryId||'?')}</span>`","`<span class=\"badge ok\">${tr('model_editor.geometry_compare.instance','INSTANCE')} → ${escapeMarkup(r.instanceOfGeometryId||'?')}</span>`")
h=h.replace("`<div class=\"maintenanceClean\" style=\"padding:8px\">✓ SOURCE CURRENT · all stored file hashes match</div>`","`<div class=\"maintenanceClean\" style=\"padding:8px\">✓ ${tr('model_editor.maintenance.source_current_detail','SOURCE CURRENT · all stored file hashes match')}</div>`")

# Surfaces one-row check title.
h=h.replace("`SURFACES CHECK: ${stageValue.toUpperCase()}\\n${issues.length?issues.join('\\n'):","`${tr('model_editor.surfaces.surface_check','SURFACES CHECK: {status}',{status:stageValue.toUpperCase()})}\\n${issues.length?issues.join('\\n'):")

# Overlay: one whole localized template, preserving technical values.
old="$('overlay').textContent=`render LOD${state.activeLod}: ${rn.id}\ngeometry: ${rn.geometryIndex>=0?'G'+rn.geometryIndex:'none'}\nsemantic: ${rn.semanticNodeIndex>=0?state.asset.nodes[rn.semanticNodeIndex]?.id:'unbound'}`;"
new="$('overlay').textContent=tr('model_editor.overlay.render_detail','render LOD{lod}: {id}\\ngeometry: {geometry}\\nsemantic: {semantic}',{lod:state.activeLod,id:rn.id,geometry:rn.geometryIndex>=0?'G'+rn.geometryIndex:tr('model_editor.common.none','none'),semantic:rn.semanticNodeIndex>=0?state.asset.nodes[rn.semanticNodeIndex]?.id:tr('model_editor.overlay.unbound','unbound')});"
h=rep(h,old,new,'render overlay')

# Geometry inventory toolbar and dynamic controls.
old="root.innerHTML=`<div class=\"geometryInventoryLods\">${lods.map((item,i)=>`<button type=\"button\" class=\"geometryInventoryMini ${i===state.activeLod?'active ':''}${item?.loaded?'':'unloaded'}\" data-geometry-inventory-lod=\"${i}\" title=\"${item?.loaded?'VIEW':'LOAD'} LOD${i}\">LOD${i}</button>`).join('')}</div><div class=\"geometryInventoryVisibility\"><button type=\"button\" class=\"geometryInventoryMini visibilityAction\" data-geometry-all-on>● ALL</button><button type=\"button\" class=\"geometryInventoryMini visibilityAction\" data-geometry-all-off>○ NONE</button><span class=\"geometryInventoryCount\">${visible}/${total} VISIBLE</span></div>`;"
new="root.innerHTML=`<div class=\"geometryInventoryLods\">${lods.map((item,i)=>`<button type=\"button\" class=\"geometryInventoryMini ${i===state.activeLod?'active ':''}${item?.loaded?'':'unloaded'}\" data-geometry-inventory-lod=\"${i}\" title=\"${escapeMarkup(item?.loaded?tr('model_editor.geometry_inventory.view_lod','VIEW LOD{lod}',{lod:i}):tr('model_editor.geometry_inventory.load_lod','LOAD LOD{lod}',{lod:i}))}\">LOD${i}</button>`).join('')}</div><div class=\"geometryInventoryVisibility\"><button type=\"button\" class=\"geometryInventoryMini visibilityAction\" data-geometry-all-on>${tr('model_editor.geometry_inventory.all','● ALL')}</button><button type=\"button\" class=\"geometryInventoryMini visibilityAction\" data-geometry-all-off>${tr('model_editor.geometry_inventory.none','○ NONE')}</button><span class=\"geometryInventoryCount\">${tr('model_editor.geometry_inventory.visible','{visible}/{total} VISIBLE',{visible,total})}</span></div>`;"
h=rep(h,old,new,'geometry inventory toolbar')
h=h.replace("eye.title=geometryInventoryVisibilitySet(state.activeLod,false)===null?'Click to isolate this mesh family':(visible?'Hide this mesh family':'Show this mesh family');","eye.title=geometryInventoryVisibilitySet(state.activeLod,false)===null?tr('model_editor.geometry_inventory.isolate_tip','Click to isolate this mesh family'):(visible?tr('model_editor.geometry_inventory.hide_tip','Hide this mesh family'):tr('model_editor.geometry_inventory.show_tip','Show this mesh family'));")
h=h.replace("checkbox.setAttribute('aria-label',`${visible?'Hide':'Show'} ${g.id}`);","checkbox.setAttribute('aria-label',tr(visible?'model_editor.common.hide_item':'model_editor.common.show_item',visible?'Hide {id}':'Show {id}',{id:g.id}));")
h=h.replace("link.textContent='INSTANCE';","link.textContent=tr('model_editor.geometry_compare.instance','INSTANCE');")
h=h.replace("link.title=`Shared geometry properties come from ${effectiveId}. SOURCE provenance remains ${g.sourcePath||g.id}.`;","link.title=tr('model_editor.geometry_inventory.shared_tip','Shared geometry properties come from {id}. SOURCE provenance remains {source}.',{id:effectiveId,source:g.sourcePath||g.id});")
h=h.replace("missing.textContent='SOURCE MISSING · LINK KEPT';","missing.textContent=tr('model_editor.source.missing_link_kept','SOURCE MISSING · LINK KEPT');")
h=h.replace("reload.textContent='↻ SOURCE';","reload.textContent=tr('model_editor.source.reload','↻ SOURCE');")
h=h.replace("reload.title=`Reload only this mesh from linked SOURCE OBJ\\n${g.sourcePath}\\nThis is not RELOAD LOD.`;","reload.title=`${tr('model_editor.geometry_inventory.reload_tip','Reload only this mesh from linked SOURCE OBJ. This is not RELOAD LOD.')}\\n${g.sourcePath}`;")
h=h.replace("if(loaded)io.title=`RELOAD LOD${li} from saved WORKING/PRODUCTION .elmesh. Never reads SOURCE OBJ.`;","if(loaded)io.title=tr('model_editor.lod.reload_native_tip','RELOAD LOD{lod} from saved WORKING/PRODUCTION .elmesh. Never reads SOURCE OBJ.',{lod:li});")

# Save stamp tooltip.
old="e.title=`Loaded save revision: ${rev>0?'r'+rev:'legacy/unsaved'}\\nSaved: ${local}\\nAuthority on OPEN: ${authority}${source?'\\nSOURCE folder: '+source:''}`;"
new="e.title=tr('model_editor.save_stamp.title','Loaded save revision: {revision}\\nSaved: {saved}\\nAuthority on OPEN: {authority}{source}',{revision:rev>0?'r'+rev:tr('model_editor.save_stamp.legacy_unsaved','legacy/unsaved'),saved:local,authority,source:source?tr('model_editor.save_stamp.source_suffix','\\nSOURCE folder: {source}',{source}):''});"
h=rep(h,old,new,'save stamp tooltip')

# Pass localized state-variant badge and validation row labels into pure builders.
h=h.replace("damageStateVariantsHtml(model,{empty:tr('model_editor.v4.states.none','No damage/repair variants yet. Select a semantic node to add one.')})","damageStateVariantsHtml(model,{empty:tr('model_editor.v4.states.none','No damage/repair variants yet. Select a semantic node to add one.'),detached:tr('model_editor.common.detached','DETACHED')})")
h=h.replace("none:tr('model_editor.wizard.validate_none','Press CHECK to run validation.')};","none:tr('model_editor.wizard.validate_none','Press CHECK to run validation.'),rowPass:tr('model_editor.common.pass','PASS'),rowFail:tr('model_editor.common.fail','FAIL')};")
HTML.write_text(h,encoding='utf-8')

# ----------------------------- PURE builders -----------------------------
p=WEB/'damage/state_variants.js'; t=p.read_text(encoding='utf-8')
t=t.replace("${row.nodeId}${row.detached?' · DETACHED':''}","${row.nodeId}${row.detached?' · '+text.detached:''}")
p.write_text(t,encoding='utf-8')

p=WEB/'final_assembly/validation.js'; t=p.read_text(encoding='utf-8')
t=t.replace("${x.passed?'PASS':'FAIL'}","${x.passed?text.rowPass:text.rowFail}")
p.write_text(t,encoding='utf-8')

p=WEB/'core/surfaces.js'; t=p.read_text(encoding='utf-8')
t=t.replace("'<option value=\"\" selected disabled>— MIXED —</option>'","`<option value=\"\" selected disabled>${text.mixed}</option>`")
t=t.replace("?' · USED HERE':''","?' · '+text.usedHereTag:''")
t=t.replace("<b class=\"grow\">ACTIVE LOD MESHES / ${text.geometry}</b>","<b class=\"grow\">${text.activeMeshes}</b>")
t=t.replace(">● ПОКАЗАТЬ ВСЕ</button>",">${text.showAll}</button>")
t=t.replace(">○ СПРЯТАТЬ ВСЕ</button>",">${text.hideAll}</button>")
t=t.replace("<span class=\"surfaceVisibleCount\">${model.visibleGeometryCount}/${model.rowCount} VISIBLE</span>","<span class=\"surfaceVisibleCount\">${text.visibleCount}</span>")
t=t.replace("<div class=\"sectionHint\" style=\"padding:6px 8px\">Click = one mesh · Ctrl = add/remove one · Shift = select range. Surface type applies to the whole selected group; material editing stays on the primary row.</div>","<div class=\"sectionHint\" style=\"padding:6px 8px\">${text.selectionHelp}</div>")
t=t.replace("${model.selectedGeometries.length} MESHES","${text.meshesCount}")
t=t.replace("<span class=\"badge\">PRIMARY</span>","<span class=\"badge\">${text.primary}</span>")
t=t.replace("` · APPLY TO ${model.selectedGeometries.length} SELECTED`","` · ${text.applySelected}`")
t=t.replace("'<span class=\"badge\">PRIMARY MESH ONLY</span>'","`<span class=\"badge\">${text.primaryOnly}</span>`")
p.write_text(t,encoding='utf-8')

# Add surface text values in caller object without making the pure builder depend on i18n.
h=HTML.read_text(encoding='utf-8')
needle="intentLabels:{auto:tr('model_editor.surfaces.intent_auto','AUTO / source')"
add="mixed:tr('model_editor.surfaces.mixed','— MIXED —'),usedHereTag:tr('model_editor.surfaces.used_here_tag','USED HERE'),activeMeshes:tr('model_editor.surfaces.active_meshes','ACTIVE LOD MESHES / {geometry}',{geometry:tr('model_editor.surfaces.geometry','GEOMETRY SURFACES')}),showAll:tr('model_editor.action.show_all','● SHOW ALL'),hideAll:tr('model_editor.action.hide_all','○ HIDE ALL'),visibleCount:tr('model_editor.surfaces.visible','{visible}/{total} VISIBLE',{visible:surfaceDisplayRows.length?surfaceDisplayRows.filter(r=>surfaceGeometryVisible(r)).length:0,total:surfaceDisplayRows.length}),selectionHelp:tr('model_editor.surfaces.selection_help','Click = one mesh · Ctrl = add/remove one · Shift = select range. Surface type applies to the whole selected group; material editing stays on the primary row.'),meshesCount:tr('model_editor.common.meshes_count','{count} MESHES',{count:surfaceSelectionIds(lod).size}),primary:tr('model_editor.common.primary','PRIMARY'),applySelected:tr('model_editor.common.apply_selected','APPLY TO {count} SELECTED',{count:surfaceSelectionIds(lod).size}),primaryOnly:tr('model_editor.common.primary_mesh_only','PRIMARY MESH ONLY'),"
# The model data needed by these values is only available after surfaceDisplayRows is declared, so inject later before builder call instead.
# Revert strategy: extend text after surfaceDisplayRows is known.
insert_point="const geometries=lod?.geometries||[],surfaceDisplayRows=logicalGeometryRows(lod,state.activeLod,state.asset?.meshSourceRecords),materials=state.asset.materials||[],selectedIds=surfaceSelectionIds(lod);"
addition=insert_point+"Object.assign(text,{mixed:tr('model_editor.surfaces.mixed','— MIXED —'),usedHereTag:tr('model_editor.surfaces.used_here_tag','USED HERE'),activeMeshes:tr('model_editor.surfaces.active_meshes','ACTIVE LOD MESHES / {geometry}',{geometry:tr('model_editor.surfaces.geometry','GEOMETRY SURFACES')}),showAll:tr('model_editor.action.show_all','● SHOW ALL'),hideAll:tr('model_editor.action.hide_all','○ HIDE ALL'),visibleCount:tr('model_editor.surfaces.visible','{visible}/{total} VISIBLE',{visible:surfaceDisplayRows.filter(r=>surfaceGeometryVisible(r)).length,total:surfaceDisplayRows.length}),selectionHelp:tr('model_editor.surfaces.selection_help','Click = one mesh · Ctrl = add/remove one · Shift = select range. Surface type applies to the whole selected group; material editing stays on the primary row.'),meshesCount:tr('model_editor.common.meshes_count','{count} MESHES',{count:selectedIds.size}),primary:tr('model_editor.common.primary','PRIMARY'),applySelected:tr('model_editor.common.apply_selected','APPLY TO {count} SELECTED',{count:selectedIds.size}),primaryOnly:tr('model_editor.common.primary_mesh_only','PRIMARY MESH ONLY')});"
h=rep(h,insert_point,addition,'surface localized builder text')
HTML.write_text(h,encoding='utf-8')

# ----------------------------- SEMANTICS physical-size block -----------------------------
p=WEB/'effects/semantics.js'; t=p.read_text(encoding='utf-8')
old=re.search(r"function physicalSizePanelHtml\(\)\{.*?\}\nfunction bindPhysicalSizePanel",t,re.S)
if not old: raise RuntimeError('physicalSizePanelHtml not found')
new=r'''function physicalSizePanelHtml(){const p=state.asset?.physicalSize||{},a=Array.isArray(p.authoringExtents)?p.authoringExtents:[0,0,0],m=Array.isArray(p.physicalExtents)?p.physicalExtents:[0,0,0],g=Array.isArray(p.gameDimensionsMeters)?p.gameDimensionsMeters:[0,0,0],axis=String(p.axis||'z'),axisIndex=axis==='x'?0:axis==='y'?1:2,linked=!!p.gameLinked,targetRaw=Number(p.targetMeters),linkedTarget=Number(g[axisIndex]||0),target=Number.isFinite(targetRaw)&&targetRaw>0?targetRaw:(linkedTarget>0?linkedTarget:Math.max(Number(a[axisIndex]||0),1)),scale=Number(p.sourceToMeters||1),enabled=!!p.enabled,legacy=!!p.legacyMigrationRequired,fmt=(v,n=3)=>Number.isFinite(Number(v))?Number(v).toFixed(n):'—';
 const game=linked?`<div class="wizardGrid physicalSizeCurrent"><span>${tr('model_editor.physical_scale.game_width','GAME WIDTH · X')}</span><span class="value">${fmt(g[0])} m</span><span>${tr('model_editor.physical_scale.game_height','GAME HEIGHT · Y')}</span><span class="value">${fmt(g[1])} m</span><span>${tr('model_editor.physical_scale.game_length','GAME LENGTH · Z')}</span><span class="value">${fmt(g[2])} m</span></div>`:`<div class="semanticHint">${tr('model_editor.physical_scale.game_not_linked','GAME LINK · NOT LINKED')}</div>`;
 const physical=enabled?`<div class="wizardGrid physicalSizeCurrent"><span>${tr('model_editor.physical_scale.physical_x','PHYSICAL X')}</span><span class="value">${fmt(m[0])} m</span><span>${tr('model_editor.physical_scale.physical_y','PHYSICAL Y')}</span><span class="value">${fmt(m[1])} m</span><span>${tr('model_editor.physical_scale.physical_z','PHYSICAL Z')}</span><span class="value">${fmt(m[2])} m</span><span>${tr('model_editor.physical_scale.source_to_meters','SOURCE → METERS')}</span><span class="value">×${Number(scale).toPrecision(9)}</span><span>${tr('model_editor.physical_scale.calibration','CALIBRATION')}</span><span class="value">${fmt(p.sourceExtent,6)} u → ${fmt(p.targetMeters,6)} m</span></div>`:`<div class="semanticHint">${tr('model_editor.physical_scale.not_calibrated','PHYSICAL SCALE · NOT CALIBRATED. WORKING geometry remains raw SOURCE coordinates.')}</div>`;
 const legacyWarning=legacy?`<div class="statusBanner danger">${tr('model_editor.physical_scale.legacy_warning','LEGACY DESTRUCTIVE SCALE STATE · incremental SOURCE scan/reload is blocked. Use RELOAD ALL SOURCE MESHES once to migrate back to raw authoring space, then SAVE.')}</div>`:'';
 return `<div class="maintenanceBlock physicalSizeBlock"><div class="geometryBlockHead"><span class="grow">${tr('model_editor.physical_scale.title','PHYSICAL SCALE · AUTHORING → METERS')}</span><span class="badge">${escapeMarkup(String(p.geometrySpace||'authoring').toUpperCase())}</span></div><div class="semanticHint">${tr('model_editor.physical_scale.help','SOURCE and WORKING are never resized. Set one asset-wide coefficient manually; BUILD applies it once to a temporary production copy for renderer/physics.')}</div>${legacyWarning}<div class="geometryBlockHead"><span class="grow">${tr('model_editor.physical_scale.authoring_size','AUTHORING SIZE · RAW SOURCE SPACE')}</span></div><div class="wizardGrid physicalSizeCurrent"><span>X</span><span class="value">${fmt(a[0])} u</span><span>Y</span><span class="value">${fmt(a[1])} u</span><span>Z</span><span class="value">${fmt(a[2])} u</span></div><div class="geometryBlockHead"><span class="grow">${tr('model_editor.physical_scale.game_size','GAME SIZE')}</span></div>${game}<div class="physicalSizeControls"><label class="physicalSizeControl"><span>${tr('model_editor.physical_scale.reference_axis','REFERENCE AXIS')}</span><select id="physicalSizeAxis"><option value="x" ${axis==='x'?'selected':''}>X</option><option value="y" ${axis==='y'?'selected':''}>Y</option><option value="z" ${axis==='z'?'selected':''}>Z</option></select></label><label class="physicalSizeControl"><span>${tr('model_editor.physical_scale.extent','PHYSICAL EXTENT')}</span><span class="physicalSizeTargetWrap"><input id="physicalSizeTarget" type="number" min="0.000001" step="0.01" value="${target}"><span class="physicalSizeUnit">m</span></span></label></div>${physical}<div class="physicalSizeActions"><button id="physicalSizeApply" class="wizardPrimary" ${legacy?'disabled':''}>${tr('model_editor.physical_scale.set_contract','↔ SET SCALE CONTRACT')}</button></div></div>`;}
function bindPhysicalSizePanel'''
t=t[:old.start()]+new+t[old.end():]
p.write_text(t,encoding='utf-8')

# ----------------------------- LOD runtime -----------------------------
p=WEB/'effects/lod_runtime.js'; t=p.read_text(encoding='utf-8')
old="const name=info?`SELECTED · LOD${info.lodIndex}/RN${info.renderNodeIndex}${info.geometryIndex>=0?`/G${info.geometryIndex}`:''} · ${escapeMarkup(info.node?.id||'?')}${alias?` · INSTANCE ${escapeMarkup(alias.geometryId)} → ${escapeMarkup(alias.instanceOfGeometryId)}`:''}`:tr('model_editor.lod.preflight.select_hint','SELECTED · click a row or mesh');"
new="const name=info?tr('model_editor.lod.preflight.selected','SELECTED · LOD{lod}/RN{rn}{geometry} · {id}',{lod:info.lodIndex,rn:info.renderNodeIndex,geometry:info.geometryIndex>=0?`/G${info.geometryIndex}`:'',id:escapeMarkup(info.node?.id||'?')})+(alias?tr('model_editor.lod.preflight.instance_suffix',' · INSTANCE {from} → {to}',{from:escapeMarkup(alias.geometryId),to:escapeMarkup(alias.instanceOfGeometryId)}):''):tr('model_editor.lod.preflight.select_hint','SELECTED · click a row or mesh');"
t=rep(t,old,new,'preflight selected label')
t=t.replace("const meshLabel=(m)=>`${m.isSourceVariant?'↳ REPLACEMENT':'MAIN'} · G${Number(m.geometryIndex)} · ${m.geometryId} · ${Number(m.triangles||0).toLocaleString()} T`;","const meshLabel=(m)=>tr('model_editor.lod_generator.mesh_label','{kind} · G{geometry} · {id} · {triangles} T',{kind:m.isSourceVariant?'↳ '+tr('model_editor.common.replacement','REPLACEMENT'):tr('model_editor.common.main','MAIN'),geometry:Number(m.geometryIndex),id:m.geometryId,triangles:Number(m.triangles||0).toLocaleString()});")
t=t.replace('title="VIEW"><input type="checkbox" disabled title="LOD0 is authoritative"','title="${tr(\'model_editor.lod_generator.view_title\',\'VIEW\')}"><input type="checkbox" disabled title="${tr(\'model_editor.lod_generator.lod0_authoritative\',\'LOD0 is authoritative\')}"')
t=t.replace("${triangleBudget(total)} · AUTHORED","${triangleBudget(total)} · ${tr('model_editor.lod_generator.authored','AUTHORED')}")
t=t.replace('title="VIEW" ${state.lodGeneratorApplying?\'disabled\':\'\'}','title="${tr(\'model_editor.lod_generator.view_title\',\'VIEW\')}" ${state.lodGeneratorApplying?\'disabled\':\'\'}')
t=t.replace('<div class="lodGenHeader"><span>VIEW</span><span>USE</span><span>LOD</span>',"<div class=\"lodGenHeader\"><span>${tr('model_editor.lod_generator.header_view','VIEW')}</span><span>${tr('model_editor.lod_generator.header_use','USE')}</span><span>LOD</span>")
t=t.replace("orientationStale?'MANUAL FLIP · STALE':'MANUAL FLIP'","orientationStale?tr('model_editor.lod.preflight.manual_flip_stale','MANUAL FLIP · STALE'):tr('model_editor.lod.preflight.manual_flip','MANUAL FLIP')")
p.write_text(t,encoding='utf-8')

# Strengthen contract around the newly migrated visible paths.
p=TEST; t=p.read_text(encoding='utf-8')
anchor="if errors:\n"
checks="""for required in [
 'model_editor.physical_scale.title','model_editor.surfaces.selection_help','model_editor.geometry_inventory.shared_tip',
 'model_editor.maintenance.scan_metrics','model_editor.overlay.render_detail','model_editor.common.detached'
]:
 if required not in catalog.get('strings',{}): errors.append(f'visible localization key missing: {required}')
for forbidden in [
 'GAME LINK · NOT LINKED — с игрой пока связи нет.',
 'PHYSICAL SCALE · NOT CALIBRATED. WORKING geometry remains raw SOURCE coordinates.',
 'Click = one mesh · Ctrl = add/remove one · Shift = select range.',
 'Shared geometry properties come from ${effectiveId}. SOURCE provenance remains',
 'SOURCE CURRENT · all stored file hashes match</div>',
 'render LOD${state.activeLod}: ${rn.id}\\ngeometry:'
]:
 if forbidden in html or forbidden in semantics or forbidden in lod_runtime: errors.append(f'visible English bypass remains: {forbidden}')
"""
if checks not in t: t=t.replace(anchor,checks+anchor)
p.write_text(t,encoding='utf-8')
print('MAE visible localization completion staged')
