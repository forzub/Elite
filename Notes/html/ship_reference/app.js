
const D=window.ELITE_SHIP_REF_DATA;
const $=(s,r=document)=>r.querySelector(s);
const esc=s=>String(s??"").replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#39;"}[c]));
const fmt=n=>typeof n==="number"?new Intl.NumberFormat("ru-RU",{maximumFractionDigits:3}).format(n):String(n??"—");
const partMap=new Map(D.parts.map(p=>[p.id,p]));
const shipMap=new Map(D.ships.map(s=>[s.id,s]));
const laserMap=new Map(D.weapons.lasers.map(x=>[x.id,x]));
const missileMap=new Map(D.weapons.missiles.map(x=>[x.id,x]));
function kv(k,v){return '<div class="kv"><div class="k">'+esc(k)+'</div><div>'+v+'</div></div>'}
function conceptHTML(s){
 const b=s.design_language?.block||{},m=s.design_language?.manufacturer||{};
 return '<div class="card concept"><h3>Концепция внешнего вида</h3>'+
 '<p><b>Блок:</b> '+esc(b.title||s.cooperation_group)+'</p>'+
 '<p><b>Корпус:</b> '+esc(m.construction||b.hull||"—")+'</p>'+
 '<p><b>Принцип:</b> '+esc(b.philosophy||"—")+'</p>'+
 '<p><b>Стоимость/уровень:</b> '+esc(m.cost||b.quality||"—")+'</p>'+
 '<p><b>Визуальная подпись:</b> '+esc(m.signature||b.visual||"—")+'</p>'+
 '<p><b>Резервирование:</b> '+esc(b.redundancy||"—")+'</p></div>';
}
function sketch(part){
 const g=part.geometry||{},e=g.envelope_m||[1,1,1],w=e[0]||1,h=e[1]||1,l=e[2]||1;
 const max=Math.max(w,h,l,.01),sx=190*w/max,sy=130*h/max,sl=110*l/max,x=95,y=70,dx=sl*.38,dy=-sl*.24;
 let extra='';
 if(part.cat==="drive") extra='<ellipse cx="'+(x+sx)+'" cy="'+(y+sy/2)+'" rx="12" ry="'+Math.max(10,sy*.34)+'" fill="none" stroke="#73c8ff" stroke-width="2"/>';
 if(part.cat==="reactor"&&g.cooling_pad_side_m) extra+='<rect x="24" y="236" width="170" height="18" fill="none" stroke="#9be28f"/><text x="109" y="274" text-anchor="middle" fill="#9be28f" font-size="12">cooling pad '+esc(g.cooling_pad_side_m)+' × '+esc(g.cooling_pad_side_m)+' m</text>';
 if(part.cat==="radiator") extra+='<line x1="25" y1="225" x2="210" y2="225" stroke="#9be28f" stroke-width="8"/>';
 return '<svg class="sketch" viewBox="0 0 420 310" xmlns="http://www.w3.org/2000/svg"><g stroke="#5f8eaa" fill="#142330" stroke-width="1.5"><rect x="'+x+'" y="'+y+'" width="'+sx+'" height="'+sy+'"/><path d="M '+x+' '+y+' l '+dx+' '+dy+' h '+sx+' l '+(-dx)+' '+(-dy)+' M '+(x+sx)+' '+y+' l '+dx+' '+dy+' v '+sy+' l '+(-dx)+' '+(-dy)+' M '+x+' '+(y+sy)+' l '+dx+' '+dy+' h '+sx+'"/></g>'+extra+'<g fill="#c7dbe8" font-size="12" font-family="Consolas,monospace"><text x="18" y="24">'+esc(part.name)+'</text><text x="18" y="44">W '+fmt(w)+' m   H '+fmt(h)+' m   L '+fmt(l)+' m</text><text x="18" y="294">service envelope: '+esc((g.service_clearance_m||[]).map(fmt).join(" × "))+' m</text></g></svg>';
}
function missileSketch(m){
 const g=m.geometry||{},L=g.length_m||2,Dm=g.diameter_m||.2;
 return '<svg class="sketch" viewBox="0 0 420 180"><g stroke="#73c8ff" fill="#142330" stroke-width="2"><path d="M70 90 L110 55 H300 L355 90 L300 125 H110 Z"/><line x1="110" y1="55" x2="110" y2="125"/></g><g fill="#c7dbe8" font-size="12" font-family="Consolas,monospace"><text x="18" y="24">'+esc(m.name)+'</text><text x="18" y="44">L '+fmt(L)+' m   Ø '+fmt(Dm)+' m   mass '+fmt(m.mass_kg)+' kg</text></g></svg>';
}
function weaponSummary(s){
 const w=s.weapons||{},rows=[];
 for(const x of w.laser_emitters||[]){const l=x.laser||{};rows.push('<div class="weaponRow"><div><span class="badge">ЛАЗЕР</span><br>'+x.emitters+' излуч.</div><div><b>'+esc(l.name)+'</b> · '+fmt(l.power_mw)+' МВт · '+fmt(l.range_km)+' км · луч r='+fmt(l.beam_radius_m)+' м</div></div>')}
 for(const x of w.missile_cells||[]) rows.push('<div class="weaponRow"><div><span class="badge">РАКЕТЫ</span><br>'+x.cells+' ячеек</div><div>'+x.compatible_missiles.map(m=>'<b>'+esc(m.name)+'</b> '+fmt(m.mass_kg)+' кг / '+fmt(m.range_km)+' км').join('<br>')+'</div></div>');
 for(const x of w.kinetic_mounts||[]) rows.push('<div class="weaponRow"><div><span class="badge">КИНЕТИКА</span><br>'+x.qty+' шт.</div><div>'+esc(partMap.get(x.hardware)?.name||x.hardware)+'</div></div>');
 if(w.legacy_missile_capacity) rows.push('<div class="note warnbox">Legacy runtime Cobrett: дополнительно ёмкость '+w.legacy_missile_capacity+' ракет; отдельная физическая пусковая ещё не сведена с BOM.</div>');
 return rows.length?rows.join(""):'<span class="sub">Вооружение не установлено.</span>';
}
const PROP="PROPOSAL (reference)",isProp=x=>!!x&&x.status===PROP;
const HULL_RU={variant:"вариант существующего корпуса",new:"новый корпус",craft:"малый аппарат"};
const ROLE_RU=Object.fromEntries((D.coverage?.roles||[]).map(r=>[r.id,r.ru]));
function shipLink(id){const s=shipMap.get(id);return s?'<a href="index.html?ship='+encodeURIComponent(id)+'">'+esc(s.name)+'</a>':esc(id)}
function proposalHTML(s){
 const r=s.reference;if(!r)return "";const b=r.balance||{},li=a=>a&&a.length?'<ul>'+a.map(x=>'<li>'+esc(x)+'</li>').join("")+'</ul>':'<span class="sub">—</span>';
 const ok=(v,lim)=>v==null?'—':'<span class="'+(v>=lim?'good':'danger')+'">'+fmt(v)+'</span>';
 const surf=(b.pad_installed_m2||0)+(b.radiator_area_m2||0);
 return '<div class="card proposalCard"><h3>Предложение справочника · '+esc(HULL_RU[r.hull]||r.hull)+'</h3>'+
 kv("Роль в покрытии",esc(ROLE_RU[r.role]||r.role))+
 kv("Корпус",r.variant_of?'вариант '+shipLink(r.variant_of)+' — та же сетка, другие модули':(r.hull==="new"?'<span class="warn">новая сетка</span>':'малый аппарат'))+
 kv("Трудоёмкость модели",fmt(r.model_effort_hull_eq)+" корпуса")+
 kv("Экипаж / пассажиры",fmt(r.crew)+" / "+fmt(r.passengers))+kv("Груз",fmt(r.cargo_t)+" т"+(r.c20?" · "+fmt(r.c20)+" C20":""))+
 (r.hull_structure_t!=null?kv("Масса конструкции",fmt(r.hull_structure_t)+" т (сухая = конструкция + BOM)"):"")+
 '<p>'+esc(r.design_note)+'</p><h4>Взаимозаменяемость</h4>'+li(r.interchangeability)+
 '<h4>Баланс (правила ship_designer.py)</h4><table><tr><th>Проверка</th><th>Значение</th></tr>'+
 '<tr><td>Реакторы / тяга + пик RCS</td><td>'+fmt(b.propulsion_reactor_mw)+' / '+fmt(b.propulsion_demand_mw)+' МВт = '+ok(b.propulsion_ratio,1)+'</td></tr>'+
 '<tr><td>Реактор на группу / спрос группы</td><td>'+fmt(b.reactor_per_group_mw)+' / '+fmt(b.group_demand_mw)+' МВт</td></tr>'+
 '<tr><td>Служебная мощность / нагрузка (оружие 30 % · 100 %)</td><td>'+fmt(b.service_capacity_mw)+' / '+fmt(b.hotel_mw_weapons30)+' · '+fmt(b.hotel_mw_weapons100)+' МВт · запас при полном огне '+ok(b.service_ratio_weapons100,1)+'</td></tr>'+
 '<tr><td>Радиаторы / служебное тепло (30 % · 100 %)</td><td>'+fmt(b.radiator_mw)+' / '+fmt(b.service_heat_mw_weapons30)+' · '+fmt(b.service_heat_mw_weapons100)+' МВт · запас при полном огне '+ok(b.radiator_ratio_weapons100,1)+'</td></tr>'+
 '<tr><td>Контуры охлаждения</td><td>'+fmt(b.coolant_loop_mw)+' МВт</td></tr>'+
 '<tr><td>Пятаки реакторов: нужно / стоит</td><td>'+fmt(b.pad_required_m2)+' / '+fmt(b.pad_installed_m2)+' м²</td></tr>'+
 '<tr><td>Пятаки + радиаторы / план корпуса</td><td>'+fmt(surf)+' / '+fmt(b.plan_area_m2)+' м² ('+fmt(Math.round((b.cooling_surface_share_of_plan||0)*100))+' %)</td></tr>'+
 '<tr><td>Полная масса / ускорение</td><td>'+fmt(b.gross_t)+' т · '+fmt(b.accel_g_gross)+' g</td></tr>'+
 '<tr><td>Ядро прыжка / полная масса</td><td>'+(b.jump_core_rating_t?fmt(b.jump_core_rating_t)+' / '+fmt(b.gross_t)+' т':'без прыжка')+'</td></tr>'+
 '<tr><td>ЖО / спасательные места / людей</td><td>'+fmt(b.life_support_persons)+' / '+fmt(b.survival_seats)+' / '+fmt(b.persons)+'</td></tr></table>'+
 ((b.flags||[]).length?'<div class="note warnbox">'+b.flags.map(esc).join("<br>")+'</div>':'<div class="note">Проверки справочника пройдены.</div>')+
 '<h4>Открыто</h4>'+li(r.open)+'</div>';
}
function coverageHTML(){
 const C=D.coverage;if(!C)return '<div class="card">Нет данных покрытия.</div>';const M=C.models;
 const cell=c=>{if(!c)return '—';const h=[];for(const id of c.existing||[])h.push('<div class="cv ex">● '+shipLink(id)+'</div>');for(const id of c.added||[])h.push('<div class="cv add">◆ '+shipLink(id)+'</div>');if(c.partial)h.push('<div class="cv part">'+(c.partial.trim()==="—"?"—":'◐ '+esc(c.partial))+'</div>');return h.join("")||'—'};
 let t='<table class="coverage"><tr><th>Роль</th>'+C.blocs.map(b=>'<th>'+esc(b.ru)+'</th>').join("")+'</tr>';
 for(const r of C.roles)t+='<tr><td class="role">'+esc(r.ru)+'</td>'+(r.all?'<td colspan="'+C.blocs.length+'">'+cell(r.all)+'<span class="sub">общие для всех блоков</span></td>':C.blocs.map(b=>'<td>'+cell(r.cells[b.id])+'</td>').join(""))+'</tr>';
 t+='</table>';
 const props=D.ships.filter(isProp),np=D.parts.filter(isProp);
 return '<div class="partHead"><div class="grow"><h2>Покрытие флота: блок × роль</h2><div class="sub">'+esc(C.note)+'</div></div></div>'+
 '<div class="card covTable">'+t+'</div>'+
 '<div class="grid3"><div class="card"><h3>Сколько моделей рисовать</h3>'+
 kv("Большие корпуса",'<b>'+fmt(M.total_large_hull_meshes)+'</b> = '+fmt(M.existing_hull_meshes)+' существующих + '+fmt(M.added_new_hulls.length)+' новый ('+M.added_new_hulls.map(shipLink).join(", ")+')')+
 kv("Уже в рантайме",fmt(M.existing_hull_meshes_in_runtime)+' (Cobrett-I) → новых сеток '+fmt(M.total_large_hull_meshes-M.existing_hull_meshes_in_runtime))+
 kv("Варианты (та же сетка)",fmt(M.added_variants.length)+' шт. ≈ '+fmt(M.variant_effort_hull_eq)+' корпуса работы (модули, кит-баш, текстуры)')+
 kv("Малые аппараты",fmt(M.total_small_craft_meshes)+' ('+M.added_small_craft.map(shipLink).join(", ")+')')+
 kv("Паспортов всего",fmt(M.existing_passports)+' + '+fmt(M.added_passports)+' = '+fmt(M.total_passports))+
 '<div class="note">'+esc(M.note)+'</div></div>'+
 '<div class="card"><h3>Новые детали (PROPOSAL)</h3>'+np.map(p=>'<p><b>'+esc(p.name)+'</b><br><span class="partId">'+esc(p.id)+'</span> · '+fmt(p.mass_t)+' т · '+(p.mwe?fmt(p.mwe)+' МВт(э)':fmt(p.power_mw)+' МВт')+'<br><span class="sub">'+esc(p.notes)+'</span></p>').join("")+'</div>'+
 '<div class="card"><h3>Добавленные корабли</h3>'+props.map(s=>'<div class="cv add">◆ '+shipLink(s.id)+' <span class="sub">· '+esc(HULL_RU[s.reference.hull]||"")+(s.reference.variant_of?' '+esc(shipMap.get(s.reference.variant_of)?.name||""):"")+'</span></div>').join("")+'</div></div>';
}
function mainShipCard(s){
 const r=s.replacements||{};
 return '<div class="partHead"><div class="grow"><h2>'+esc(s.name)+'</h2><div class="sub">'+(isProp(s)?'<span class="badge prop">PROPOSAL (reference)</span> ':'')+esc(s.economic_class)+' · '+esc(s.flight.law)+'-'+esc(s.flight.layout)+' · '+esc(s.cooperation_group)+'</div></div><a class="btn mini" href="ship.html?id='+encodeURIComponent(s.id)+'">Открыть сборку →</a></div>'+
 conceptHTML(s)+
 '<div class="grid2"><div class="card"><h3>Паспорт</h3>'+
 kv("Назначение",esc(s.purpose))+kv("Сухая масса",fmt(s.mass.dry_original_t)+" т")+kv("Корпус / габарит",(s.frame.envelope_m||[]).map(fmt).join(" × ")+" м")+
 kv("Главные двигатели",fmt(s.flight.installed_main_engines)+" шт.")+kv("Одновременно работают",fmt(s.flight.simultaneous_main_engines)+" шт.")+
 kv("Расчётная тяга",fmt(s.flight.rated_directional_thrust_mn)+" МН")+kv("Потребление тяги",fmt(s.flight.rated_propulsion_input_mw)+" МВт")+
 kv("Реакторный запас",fmt(s.flight.standardized_reactor_capacity_mw)+" МВт")+kv("Прыжок",s.jump_system.fitted?'<span class="good">есть</span>':'нет')+
 '</div><div class="card"><h3>Замена корабля</h3>'+
 kv("Штатная альтернатива",r.standard?'<a href="?ship='+encodeURIComponent(r.standard.ship_id)+'">'+esc(r.standard.name)+'</a><br><span class="sub">'+esc(r.standard.reason)+'</span>':'—')+
 kv("Внешний / вражеский аналог",r.hostile_or_foreign?esc(r.hostile_or_foreign.name)+'<br><span class="sub">'+esc(r.hostile_or_foreign.reason)+'</span>':'—')+
 kv("Состояние массы",s.mass.frame_revalidation_required?'<span class="danger">рама требует перепроверки после унификации ('+fmt(s.mass.delta_fraction*100)+'%)</span>':'<span class="good">изменение '+fmt(s.mass.delta_fraction*100)+'%</span>')+
 '</div></div>'+proposalHTML(s)+'<div class="card"><h3>Вооружение</h3>'+weaponSummary(s)+'</div>'+
 '<div class="card"><h3>Основные группы оборудования</h3>'+s.functional_groups.map(g=>'<span class="pill">'+esc(g.code)+' · '+esc(g.name)+' · '+g.items.reduce((a,x)=>a+x.qty,0)+' ед.</span>').join("")+'</div>';
}
function renderMain(){
 const side=$("#shipList"),out=$("#mainContent"),q=$("#search"),groups={};
 for(const s of D.ships){const g=s.cooperation_group||"other",m=s.manufacturer||"other";(((groups[g]??={})[m]??=[])).push(s)}
 function draw(filter=""){
   const f=filter.trim().toLowerCase();let html="";
   for(const [g,ms] of Object.entries(groups)){
     const gname=D.context.design_languages.groups[g]?.title||g;let block="";
     for(const [m,ships] of Object.entries(ms)){
       const v=ships.filter(s=>!f||[s.name,s.id,s.economic_class,s.purpose].join(" ").toLowerCase().includes(f));if(!v.length)continue;
       block+='<div class="mfrTitle">'+esc(D.context.design_languages.manufacturers[m]?.title||m)+'</div>'+v.map(s=>'<button class="shipBtn'+(isProp(s)?' propBtn':'')+'" data-id="'+esc(s.id)+'">'+esc(s.name)+(isProp(s)?' <span class="badge prop">предл.</span>':'')+'<small>'+esc(s.economic_class)+' · '+esc(s.flight.law)+'-'+esc(s.flight.layout)+(isProp(s)&&s.reference.variant_of?' · вариант':'')+(isProp(s)&&s.reference.hull==="new"?' · новый корпус':'')+'</small></button>').join("");
     }
     if(block)html+='<div class="groupTitle">'+esc(gname)+'</div>'+block;
   }
   side.innerHTML='<button class="shipBtn covBtn" data-view="coverage">Покрытие флота<small>блок × роль · сколько моделей рисовать</small></button>'+html;
   side.querySelectorAll(".shipBtn[data-id]").forEach(b=>b.onclick=()=>select(b.dataset.id));side.querySelector(".covBtn").onclick=coverage;
 }
 function select(id){const s=shipMap.get(id);if(!s)return;side.querySelectorAll(".shipBtn").forEach(x=>x.classList.toggle("active",x.dataset.id===id));out.innerHTML=mainShipCard(s);out.scrollTop=0;history.replaceState(null,"","?ship="+encodeURIComponent(id))}
 function coverage(){side.querySelectorAll(".shipBtn").forEach(x=>x.classList.toggle("active",x.dataset.view==="coverage"));out.innerHTML=coverageHTML();out.scrollTop=0;history.replaceState(null,"","?view=coverage")}
 const meta=$("#refMeta"),np=D.ships.filter(isProp).length,pp=D.parts.filter(isProp).length;
 if(meta)meta.textContent=D.ships.length+" моделей (из них "+np+" предложений справочника) · "+D.parts.length+" SKU (+"+pp+" предл.)";
 q.oninput=()=>draw(q.value);draw();const sp=new URLSearchParams(location.search).get("ship");if(sp&&shipMap.has(sp))select(sp);else coverage();
}
function detailRows(p){
 const skip=new Set(["geometry","replacements","design_language","notes","manufacturer_name"]);let h="";
 for(const [k,v] of Object.entries(p)){if(skip.has(k)||v===null||typeof v==="object")continue;h+=kv(k,esc(v))}
 return h;
}
function replacementHTML(p){
 const r=p.replacements||{},one=(x,enemy)=>x?'<b>'+esc(x.name)+'</b><br><span class="'+(enemy?'danger':'good')+'">'+esc(x.part_id)+'</span><br><span class="sub">'+esc(x.note)+'</span>':'—';
 return '<div class="grid2"><div class="card"><h3>Штатная замена</h3>'+one(r.standard,false)+'</div><div class="card"><h3>Внешний / вражеский аналог</h3>'+one(r.hostile_or_foreign,true)+'</div></div>';
}
function weaponDetail(p){
 const b=D.weapons.bindings[p.id];if(!b)return "";let h='<div class="card"><h3>Боевой паспорт</h3>';
 if(b.laser_id){const l=laserMap.get(b.laser_id);h+=kv("Излучатель",esc(l.name))+kv("Излучателей на установку",fmt(b.emitters_per_mount))+kv("Мощность одного",fmt(l.power_mw)+" МВт")+kv("Дальность",fmt(l.range_km)+" км")+kv("Радиус луча",fmt(l.beam_radius_m)+" м")+sketch({...p,name:l.name,geometry:l.geometry})}
 if(b.compatible_missiles){h+=kv("Ячеек",fmt(b.cells_per_mount))+b.compatible_missiles.map(id=>{const m=missileMap.get(id);return '<h4>'+esc(m.name)+'</h4>'+kv("БЧ",esc(m.warhead))+kv("Масса",fmt(m.mass_kg)+" кг")+kv("Разгон",fmt(m.accel_mps2)+" м/с²")+kv("Vmax",fmt(m.max_speed_mps)+" м/с")+kv("Дальность",fmt(m.range_km)+" км")+missileSketch(m)}).join("")}
 return h+'<div class="note">'+esc(b.note||"")+'</div></div>';
}
function partCard(p,s,item){
 const g=p.geometry||{};
 return '<div class="partHead"><div class="grow"><h2>'+esc(p.name)+'</h2><div class="partId">'+esc(p.id)+'</div></div><span class="badge">'+esc(p.cat)+'</span></div>'+
 '<div class="grid2"><div class="card"><h3>Для моделирования</h3><div class="dim">'+esc((g.envelope_m||[]).map(fmt).join(" × "))+' м</div><div class="sub">W × H × L; заменяемый блок, не монтажная рама</div>'+sketch(p)+
 kv("Service clearance",(g.service_clearance_m||[]).map(fmt).join(" × ")+" м")+(g.cooling_pad_side_m?kv("Пятак охлаждения",fmt(g.cooling_pad_side_m)+" × "+fmt(g.cooling_pad_side_m)+" м"):"")+(g.deployed_area_m2?kv("Развёрнутая площадь",fmt(g.deployed_area_m2)+" м²"):"")+
 '</div><div class="card"><h3>Паспорт детали</h3>'+kv("На этом корабле",fmt(item.qty)+" шт.")+kv("Производитель",esc(p.manufacturer_name||p.mfr))+kv("Группа",esc(p.cooperation_group||"—"))+detailRows(p)+(p.notes?kv("Примечание",esc(p.notes)):"")+'</div></div>'+
 replacementHTML(p)+weaponDetail(p);
}
function renderShipPage(){
 const id=new URLSearchParams(location.search).get("id"),s=shipMap.get(id)||D.ships[0];
 $("#shipTitle").textContent=s.name;$("#shipMeta").textContent=s.id+" · "+s.flight.law+"-"+s.flight.layout+" · "+s.cooperation_group+(isProp(s)?" · "+PROP:"");$("#back").href="index.html?ship="+encodeURIComponent(s.id);
 const tree=$("#equipTree"),out=$("#partContent");
 tree.innerHTML='<div class="groupTitle">35 · Корпус / сборка</div><button class="treeBtn active" data-root="1">'+esc(s.name)+'<small>'+esc((s.frame.envelope_m||[]).map(fmt).join(" × "))+' м</small></button>'+
 s.functional_groups.map(g=>'<div class="treeGroup"><div class="treeGroupHead">'+esc(g.code)+' · '+esc(g.name)+'</div><div class="treeItems">'+g.items.map((x,i)=>'<button class="treeBtn" data-part="'+esc(x.part_id)+'" data-g="'+esc(g.code)+'" data-i="'+i+'">'+esc(partMap.get(x.part_id)?.name||x.part_id)+'<small>'+fmt(x.qty)+' шт. · '+esc(x.part_id)+'</small></button>').join("")+'</div></div>').join("");
 function root(){out.innerHTML=mainShipCard(s);tree.querySelectorAll(".treeBtn").forEach(x=>x.classList.toggle("active",x.dataset.root==="1"))}
 tree.querySelector('[data-root="1"]').onclick=root;
 for(const b of tree.querySelectorAll("[data-part]"))b.onclick=()=>{const g=s.functional_groups.find(x=>x.code===b.dataset.g),item=g.items[Number(b.dataset.i)],p=partMap.get(item.part_id);tree.querySelectorAll(".treeBtn").forEach(x=>x.classList.toggle("active",x===b));out.innerHTML=partCard(p,s,item)};
 root();
}
document.addEventListener("DOMContentLoaded",()=>{if(document.body.dataset.page==="index")renderMain();else renderShipPage()});
