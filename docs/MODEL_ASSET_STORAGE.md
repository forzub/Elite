# Где хранятся меши и модели (аудит 2026-11-11)

> **2026-11-13 (автор): сырьё вынесено из репозитория.** Источник для редакторов и генераторов —
> `D:/__elite/assets_source/models/` (`ELITE_ASSETS_SOURCE`); `work/assets` — таргет игры, в `assets/models` осталось
> только то, что игра грузит (Cobra `ships/cobrettymk1`, `hub/*`, `stations/LOD0`, `stations/LOD1`). Ниже везде, где
> сказано `assets/models/<группа>` про исходники (equipment, layout_sketches, ship_assets, ships кроме cobrettymk1),
> читать `assets_source/models/<группа>`. Подробно — MESH_EDITOR_SPEC.md 4 и 13.

Автор: «разобраться, где хранятся меши модулей кораблей, где сборки кораблей из модулей, где макет Cobra; навести
порядок». Ниже - что есть сейчас (по коду и диску), что опасно, и план.

## 1. Наш формат (Model Asset v4) и редактор мешей

- **Редактор:** `tools/model_asset_editor/` (C++: `ModelAssetEditorSession.cpp`, импорт OBJ, LOD, семантика), интерфейс
  `src/assets/webui/model_asset_editor.html` + `src/assets/webui/model_asset_editor/**.js`, программа
  `build/EliteAssetEditor.exe` (Windows: окно с веб-интерфейсом).
- **Формат:** `src/model_asset/ModelAsset.h` (+ `ModelAssetBinary`). Один ассет = манифест `<id>.elmodel` (семантика:
  узлы, состояния, отсеки, проёмы, коллизия, хитвалумы, сокеты, связи креплений, материалы) + по файлу на каждый LOD
  `<id>.lodN.elmesh` (геометрия и узлы отрисовки этого LOD). Два вида копий: **WORKING** (в единицах автора, редактор
  правит её) и **PRODUCTION** (после BUILD: в метрах, с измеренной ошибкой LOD; её грузит игра).
- **Исходники для редактора:** `assets/models/<группа>/<ассет>/LOD<N>/*.obj(+.mtl)` - одна папка = один ассет, части уже
  стоят на своих местах в общей системе координат (`src/assets/models/SOURCE_LAYOUT.md`). Настройки редактора:
  `build/tools/model_asset_editor/model_asset_editor.settings.json` (source = `assets/models`, compiled =
  `src/assets/compiled/models`, working = `build/tools/model_asset_editor/workspaces`).

## 2. Что сейчас где (факт)

| Что | Где лежит | Под git? | Кто читает |
|---|---|---|---|
| Меши **модулей / оборудования** (77 изделий, 4 LOD + hit_volumes.json) | `assets/models/equipment/<изделие>/` | да (201 МБ) | редактор (группа «Equipment»), генератор эскизов |
| Генератор этих мешей | `tools/models/` (eqlib.py, units/*.py, build_equipment_models.py) | да | - |
| **Cobra, которую рисует игра сейчас** (старая сборка: 32 меша частей + прокси LOD1) | `build/assets/models/ships/cobrettymk1/` - **только в папке сборки**; из `src/assets` удалена 2026-06-29 (7c57db6e); сегодня скопирована в `assets/models/ships/cobrettymk1/` | теперь да (папка ships открыта в .gitignore) | `src/game/geometry/ObjectAssemblyRegistry.cpp` (32 пути) + C++-описание `EliteCobraMk1.cpp` |
| Cobra - исходник для редактора (другая версия тех же частей!) | `assets/models/ships/cobretty-mk1-int/LOD0/` | теперь да | редактор (группа «Корабли каталога») |
| Исходники Cobra в Blender (автор) | `D:\__elite\_data\objects\ships\Cobretti-mk1*.blend` (вне репозитория) | нет | автор |
| Остальные корабли каталога (примитивы из паспортов, 20 папок) | `assets/models/ships/<корабль>/` (+ `ship_models.json`) | теперь да | редактор; игра рисует каталожные корабли блоками |
| **Эскизы компоновки** (Cobra v3 + 50 автоэскизов, OBJ по частям + рендеры PNG) | `assets/models/layout_sketches/<корабль>/` | нет (495 МБ) | справочник (рендеры), источник ассета с отсеками |
| **Макет Cobra с отсеками** (то, что я собрал) - исходник | `assets/models/ship_assets/cobrett-i/source/` (ship_asset.json + geo/*.obj) | нет (25 МБ, генерируется) | EliteShipAssetAssemble |
| ... скомпилированный | `src/assets/compiled/models/ship_cobrett_i/ship_cobrett_i.elmodel` (+ .elmesh) | нет | игра за флагом ELITE_SHIP_MODEL_ASSETS=1 |
| ... рабочая копия редактора | `build/tools/model_asset_editor/workspaces/ship_cobrett_i/working/` | нет | редактор (группа «Корабли: ассеты с отсеками») |
| Станция - исходник | `assets/models/stations/LOD0..`, `stantion-001` | нет (83 МБ) | редактор |
| Станция - скомпилированная (её рисует игра) | `src/assets/compiled/models/station/station.elmodel` + 5 .elmesh | **нет** (64 МБ) | игра (ModelAssetGpuLibrary) |
| Станция - старые сборки | `src/assets/compiled/models/station.elmodel` (под git, 69 МБ), `station_legacy_dump_v3/` | частично | никто (наследие) |
| Модули хаба (док, сервисный блок) | `assets/models/hub/` | да | игра (старая сборка), редактор |
| Рабочие копии редактора (10 вариантов Cobra и др.) | `build/tools/model_asset_editor/workspaces/` (301 МБ) | нет | редактор |

## 2а. Справочник кораблей: где его корабли и модули

Справочник (`Notes/html/ship_reference/`, открывается `index.html` / `start_server.cmd`) **не хранит 3D-моделей** - это
данные и картинки:
- **Модули (изделия):** паспорта - канон `tools/colonization_lab/canon/ship_parts_unified.json` (+ имена
  `equipment_naming.json`); в справочник собираются в `Notes/html/ship_reference/data.js` (`tools/build_proposals.js`).
  У каждого изделия поле `model_asset: "equipment/<папка>"` -> его 3D-модель лежит в `assets/models/equipment/<папка>/`
  (OBJ по LOD + hit_volumes.json). Картинки модулей - `Notes/html/ship_reference/workshop/sketches/<id изделия>/`.
- **Корабли:** паспорта - `tools/colonization_lab/canon/ship_passports_3126.json` и `ship_designs.json` (состав BOM,
  размеры, группы), тоже собираются в `data.js` (+ `layout_data.js` - раскладка отсеков). У корабля **нет своей 3D-модели
  в справочнике**: на карточке - рендеры эскиза компоновки из `workshop/sketches/hull_<id корабля>/` (JPEG), а сами
  эскизы (OBJ по частям) лежат в `assets/models/layout_sketches/<корабль>/`. Корпусные меши кораблей для редактора -
  `assets/models/ships/<папка>/` (связь паспорт -> папка: `assets/models/ships/ship_models.json`).
- Картинки и заметки мастерской: `Notes/html/ship_reference/workshop/workshop.json` + `workshop/sketches/` (54 МБ, 158
  папок).

## 3. Проблемы (почему «хаос»)

1. **Игровая Cobra существовала только в `build/`.** Сборка копирует ассеты, но не удаляет лишнее - поэтому файлы
   пережили удаление из исходников. Чистая пересборка оставила бы игру без Cobra. (Исправлено копией в
   `assets/models/ships/cobrettymk1/` + git.)
2. **Две разные Cobra:** игра - `cobrettymk1` (старая версия), редактор - `cobretty-mk1-int` (другие координаты /
   версия частей). Что из них «правильно» - знает автор.
3. **Скомпилированная станция, которую рисует игра, не под git** (`compiled/models/station/`), а под git - старый
   ненужный `station.elmodel` 69 МБ.
4. **Ни один корабль игра не грузит в нашем формате.** Станция - да (`.elmodel`), корабли - старый путь: OBJ на каждую
   часть + список путей и модулей в C++ (`ObjectAssemblyRegistry.cpp`, `EliteCobraMk1.cpp`). Отсюда же нет
   хитвалумов у ER-5 и прочих частей без мешей.
5. Меши оборудования (201 МБ OBJ) лежат в git, хотя они генерируются из `tools/models` за минуту.
6. Мёртвые пути в коде: `MeshLibrary.cpp` ссылается на `cobra-mk1.obj`, `stantion-01.obj`, `repair_drone_debug.obj`,
   которых нет нигде.
7. Эскизы и автоэскизы (495 МБ) смешаны с исходниками ассетов в `assets/models`.

## 4. Предлагаемый порядок (одно место - одно назначение)

```
assets/models/                      ИСХОДНИКИ ДЛЯ РЕДАКТОРА (OBJ по частям, по папке на ассет)
  equipment/<изделие>/              генерируется tools/models (в git - только генератор и hit_volumes; OBJ - по желанию)
  ships/<корабль>/LOD<N>/           корпус и части корабля (авторские и примитивы)
  stations/, hub/, objects/, drones/
  ship_assets/<корабль>/source/     сборка корабля из модулей (генерируется из эскиза / из редактора)
work/sketches/ (вне assets)         эскизы компоновки и рендеры для справочника (не исходники игры)
build/tools/model_asset_editor/workspaces/<id>/working/   WORKING (временная, редактор)
src/assets/compiled/models/<id>/<id>.elmodel + .lodN.elmesh   PRODUCTION - ЕДИНСТВЕННОЕ, что грузит игра (под git)
```

Шаги (каждый - отдельно, с проверкой в игре):
1. (сделано) спасти игровую Cobra в исходники; папка кораблей под git.
2. Автор выбирает «правильную» Cobra (`cobrettymk1` или `cobretty-mk1-int`), вторую - в архив.
3. Скомпилированную станцию `compiled/models/station/` - под git; старые `station.elmodel` (69 МБ) и
   `station_legacy_dump_v3` - удалить.
4. Корабли - в наш формат: редактор (или сборщик) делает `compiled/models/<корабль>/<корабль>.elmodel`, игра грузит его
   вместо C++-списка путей; хитвалумы и семантика - из ассета (это и есть срез S5).
5. Эскизы - из `assets/models/layout_sketches` в отдельную папку; мёртвые пути в `MeshLibrary.cpp` - убрать.
6. Решить, держать ли 201 МБ OBJ оборудования в git или только генератор (+ кэш).
