# Инструкция по файлу `LightingParams.h`

## Общая структура

`LightingParams` — это универсальная структура для управления визуальными параметрами объектов. Содержит настройки освещения, процедурной сетки, тумана и отрисовки ребер.

---

## 1. Базовое освещение (Fill Pass)

```cpp
glm::vec3 lightDir = glm::vec3(0.4f, 0.7f, 0.5f);   // направление света
glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);  // цвет света
glm::vec3 ambientColor = glm::vec3(0.3f, 0.3f, 0.35f); // фоновое освещение
```

**`lightDir`**  
Направление основного источника света. Вектор не обязательно нормализовать — это сделает шейдер.  
*Пример:* `(0, 1, 0)` — свет сверху.

**`lightColor`**  
Цвет света. Влияет на итоговый оттенок освещенных частей.

**`ambientColor`**  
Цвет фонового освещения (тени не будут абсолютно черными).

---

## 2. Fake Light эффекты

```cpp
float fresnelPower = 2.0f;      // степень эффекта Френеля
float rimIntensity = 0.5f;       // интенсивность подсветки краев
float edgeIntensity = 0.3f;      // не используется в текущей реализации
float normalBlend = 0.15f;       // примешивание цвета нормали
```

**`fresnelPower`**  
Управляет "резкостью" rim-эффекта. Больше значение → подсвечиваются только самые края.

**`rimIntensity`**  
Насколько сильно подсвечиваются края объекта.

**`normalBlend`**  
Добавляет легкий оттенок в зависимости от ориентации поверхности. Полезно для визуального разнообразия больших плоскостей.

---

## 3. Основные цвета объекта

```cpp
glm::vec3 hullColor = glm::vec3(0.8f, 0.8f, 0.9f);    // основной цвет
glm::vec3 detailColor = glm::vec3(0.6f, 0.6f, 0.8f);  // цвет деталей
glm::vec3 glowColor = glm::vec3(0.5f, 0.7f, 1.0f);    // цвет свечения
```

**`hullColor`**  
Базовый цвет поверхности. В шейдере смешивается с `vColor` из вершин.

**`detailColor`**, **`glowColor`**  
Зарезервированы для будущих эффектов.

---

## 4. Процедурная сетка (для больших объектов)

```cpp
struct ProceduralGrid {
    bool enabled = false;
    float cellSize = 100.0f;
    float lineThickness = 1.0f;
    glm::vec3 lineColor = glm::vec3(0.3f, 0.3f, 0.4f);
    float lineAlpha = 0.3f;
    bool fadeWithDistance = true;
    float gridGlow = 0.2f;
    glm::vec3 cellSizeXYZ = glm::vec3(100.0f);
    glm::vec3 gridOffset = glm::vec3(0.0f);
} grid;
```

**`enabled`**  
Включает/выключает отрисовку сетки.

**`cellSize`**  
Размер ячейки в мировых единицах (если все оси одинаковы).

**`cellSizeXYZ`**  
Позволяет задать разный шаг по осям.  
*Пример:* `glm::vec3(200, 100, 50)` — вытянутые ячейки.

**`lineThickness`**  
Толщина линий в пикселях.

**`lineColor`**  
Цвет линий сетки.

**`lineAlpha`**  
Прозрачность линий (0 — полностью прозрачные, 1 — непрозрачные).

**`fadeWithDistance`**  
Автоматически уменьшать прозрачность сетки с расстоянием.

**`gridGlow`**  
Добавляет мягкое свечение вокруг линий. Полезно для темных объектов.

**`gridOffset`**  
Сдвиг сетки. Можно использовать для анимации или выравнивания.

---

## 5. Distance Fade (прозрачность с расстоянием)

```cpp
struct DistanceFade {
    bool enabled = true;
    float startDistance = 500.0f;
    float endDistance = 2000.0f;
    float minAlpha = 0.0f;
    float scaleFactor = 1.0f;
} distanceFade;
```

**`enabled`**  
Включает эффект.

**`startDistance`**  
Расстояние, на котором объект начинает становиться прозрачным.

**`endDistance`**  
Расстояние полного исчезновения (`minAlpha`).

**`minAlpha`**  
Минимальная прозрачность (0 — полностью невидим, 0.2 — слегка виден).

**`scaleFactor`**  
Множитель для учета размера объекта. Для планет можно сделать < 1, чтобы они медленнее исчезали.

---

## 6. Depth Fog (туман по глубине)

```cpp
struct DepthFog {
    bool enabled = true;
    float startDistance = 300.0f;
    float endDistance = 3000.0f;
    glm::vec3 fogColor = glm::vec3(0.05f, 0.05f, 0.1f);
    bool useObjectScale = true;
} depthFog;
```

**`enabled`**  
Включает туман.

**`startDistance`**  
Расстояние начала смешивания с цветом фона.

**`endDistance`**  
Расстояние полного смешивания.

**`fogColor`**  
Цвет тумана (обычно цвет фона сцены).

**`useObjectScale`**  
Адаптировать дистанции под размер объекта.

---

## 7. Atmospheric Grid (для линий)

```cpp
struct AtmosphericGrid {
    bool enabled = true;
    float startDistance = 200.0f;
    float endDistance = 2000.0f;
    float minAlpha = 0.1f;
    float maxAlpha = 1.0f;
    bool applyToLargeObjects = true;
} atmosphericGrid;
```

*Применяется к edge-линиям, а не к процедурной сетке.*

**`enabled`**  
Включает эффект.

**`startDistance`**  
Расстояние, с которого линии начинают бледнеть.

**`endDistance`**  
Расстояние, на котором линии достигают `minAlpha`.

**`minAlpha`**, **`maxAlpha`**  
Диапазон прозрачности линий.

**`applyToLargeObjects`**  
Применять ли эффект к большим объектам.

---

## 8. Edge Pass (линии)

```cpp
glm::vec3 edgeColor = glm::vec3(0.6f, 0.7f, 1.0f);
```

**`edgeColor`**  
Базовый цвет всех линий (используется в glowPass и linePass, если не переопределен).

---

### Glow Pass

```cpp
struct GlowPass {
    float thickness = 3.0f;
    float alpha = 0.15f;
    bool enabled = true;
    bool applyFade = true;
} glowPass;
```

Широкое полупрозрачное свечение вокруг объекта.

---

### Line Pass

```cpp
struct LinePass {
    float thickness = 1.0f;
    float alpha = 1.0f;
    bool enabled = true;
    bool applyFade = true;
} linePass;
```

Основные контурные линии.

---

### Outline Pass

```cpp
struct OutlinePass {
    float thickness = 0.5f;
    float alpha = 0.8f;
    glm::vec3 color = glm::vec3(0.2f, 0.2f, 0.3f);
    bool enabled = false;
    bool applyFade = true;
} outlinePass;
```

Тонкий контур, обычно темнее основного.

---

## Примеры настройки

### 1. Черные корабли с легким fake light (темные линии)

```cpp
LightingParams blackShip() {
    LightingParams p;
    
    // Основной цвет — почти черный с легким синим оттенком
    p.hullColor = glm::vec3(0.1f, 0.1f, 0.15f);
    
    // Ребра — яркие, контрастные
    p.edgeColor = glm::vec3(0.8f, 0.9f, 1.0f);
    
    // Fake light эффекты
    p.rimIntensity = 0.3f;           // легкая подсветка краев
    p.fresnelPower = 4.0f;            // только самые края
    p.normalBlend = 0.05f;            // минимум примешивания нормалей
    
    // Свет — холодный
    p.lightColor = glm::vec3(0.8f, 0.9f, 1.0f);
    p.ambientColor = glm::vec3(0.05f, 0.05f, 0.1f);
    
    // Glow — минимальный
    p.glowPass.alpha = 0.1f;
    p.glowPass.thickness = 2.0f;
    
    return p;
}
```

### 2. Светлые корабли с темными ребрами (как сейчас)

```cpp
LightingParams lightShip() {
    LightingParams p;
    
    // Основной цвет — светлый
    p.hullColor = glm::vec3(0.9f, 0.9f, 1.0f);
    
    // Ребра — темные, контрастные
    p.edgeColor = glm::vec3(0.2f, 0.2f, 0.3f);
    
    // Fake light
    p.rimIntensity = 0.6f;
    p.fresnelPower = 2.0f;
    p.normalBlend = 0.15f;
    
    // Glow — едва заметный
    p.glowPass.alpha = 0.1f;
    p.glowPass.thickness = 2.0f;
    
    return p;
}
```

### 3. Корабль-невидимка (stealth)

```cpp
LightingParams stealthShip() {
    LightingParams p;
    
    p.hullColor = glm::vec3(0.05f, 0.05f, 0.05f);
    p.edgeColor = glm::vec3(0.1f, 0.1f, 0.15f);  // почти невидимые ребра
    
    p.rimIntensity = 0.1f;
    p.normalBlend = 0.0f;
    
    p.glowPass.enabled = false;       // без свечения
    p.linePass.alpha = 0.5f;           // полупрозрачные линии
    p.glowPass.alpha = 0.0f;
    
    return p;
}
```

### 4. Энергетический корабль (glow эффект)

```cpp
LightingParams energyShip() {
    LightingParams p;
    
    p.hullColor = glm::vec3(0.2f, 0.3f, 0.6f);
    p.edgeColor = glm::vec3(0.0f, 0.8f, 1.0f);  // ярко-голубые
    
    p.rimIntensity = 1.0f;               // максимум rim light
    p.fresnelPower = 1.5f;
    
    p.glowPass.alpha = 0.4f;              // сильное свечение
    p.glowPass.thickness = 5.0f;
    
    p.linePass.alpha = 0.8f;
    
    p.lightColor = glm::vec3(0.7f, 0.9f, 1.2f);  // холодный свет
    
    return p;
}
```

### 5. Крупный астероид с сеткой

```cpp
LightingParams asteroid(float size) {
    LightingParams p = LightingParams::largeObject(size);
    
    p.hullColor = glm::vec3(0.4f, 0.35f, 0.3f);
    p.edgeColor = glm::vec3(0.5f, 0.45f, 0.4f);
    
    // Процедурная сетка
    p.grid.enabled = true;
    p.grid.cellSize = size * 0.15f;
    p.grid.lineColor = glm::vec3(0.3f, 0.3f, 0.25f);
    p.grid.lineAlpha = 0.2f;
    p.grid.gridGlow = 0.1f;
    
    // Туман
    p.depthFog.startDistance = size * 0.5f;
    p.depthFog.endDistance = size * 3.0f;
    
    return p;
}
```

## Советы по настройке

1. **Контраст между корпусом и ребрами** создает "технический" вид:
   - Светлый корпус + темные ребра → четкие линии
   - Темный корпус + светлые ребра → светящийся контур

2. **Rim intensity** лучше держать в пределах 0.3–0.8. Выше 1.0 дает неестественное свечение.

3. **Fresnel power**:
   - 1.0–2.0 — мягкий переход
   - 3.0–5.0 — резкие края

4. **Normal blend** > 0.3 создает "радужный" эффект, подходит только для специальных случаев.

5. Для **очень больших объектов** всегда увеличивайте дистанции fog и fade пропорционально размеру.
