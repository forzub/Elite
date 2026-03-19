# Инструкция по работе с LightingParams

## 📋 Содержание
1. [Структура LightingParams](#структура-lightingparams)
2. [Параметры освещения](#параметры-освещения)
3. [Параметры корпуса](#параметры-корпуса)
4. [Дистанции и зоны](#дистанции-и-зоны)
5. [Эффекты поверхности](#эффекты-поверхности)
6. [Сетка (Grid)](#сетка-grid)
7. [Ребра (Edge)](#ребра-edge)
8. [Туман (Fog)](#туман-fog)
9. [Пресеты](#пресеты)
10. [Uniform Applicators](#uniform-applicators)
11. [Как работать с шейдерами](#как-работать-с-шейдерами)
12. [Практические примеры](#практические-примеры)

---

## Структура LightingParams

`LightingParams` - это структура, которая хранит все параметры освещения и визуальных эффектов для объектов в сцене. Она используется для передачи данных в шейдеры.

```cpp
struct LightingParams {
    // Все параметры сгруппированы по функциональности
};
```

---

## Параметры освещения

```cpp
glm::vec3 lightDir = glm::vec3(0.4f, 0.7f, 0.5f);  // направление света
glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f); // цвет света
glm::vec3 ambientColor = glm::vec3(0.3f, 0.3f, 0.35f); // фоновое освещение
```

### 📌 lightDir
- **Что это**: Направление, откуда идет основной свет
- **Формат**: Нормализованный вектор (x, y, z)
- **Как работает**: Чем ближе нормаль поверхности к направлению света, тем ярче освещение
- **Пример**: `(0.4, 0.7, 0.5)` - свет сверху-сбоку

### 📌 lightColor
- **Что это**: Цвет и интенсивность основного света
- **Формат**: (R, G, B) от 0.0 до 1.0
- **Как работает**: Умножается на цвет поверхности и на освещенность
- **Пример**: 
  - `(1.0, 1.0, 1.0)` - белый свет
  - `(1.0, 0.8, 0.6)` - теплый желтоватый

### 📌 ambientColor
- **Что это**: Фоновое освещение (свет, который есть всегда)
- **Формат**: (R, G, B) от 0.0 до 1.0
- **Как работает**: Добавляется к освещению даже в тенях
- **Важно**: Используется с коэффициентом 0.1-0.3, чтобы не пересвечивать тени

---

## Параметры корпуса

```cpp
glm::vec3 hullColor = glm::vec3(0.8f, 0.8f, 0.9f);        // основной цвет
glm::vec3 detailColor = glm::vec3(0.6f, 0.6f, 0.8f);      // цвет деталей
glm::vec3 glowColor = glm::vec3(0.5f, 0.7f, 1.0f);        // цвет свечения
```

### 📌 hullColor
- **Что это**: Основной цвет корпуса
- **Где используется**: В шейдере `mesh_fill` для кораблей

### 📌 detailColor
- **Что это**: Цвет деталей (сейчас не используется)
- **Запас**: Для будущего использования

### 📌 glowColor
- **Что это**: Цвет свечения (сейчас не используется)
- **Запас**: Для будущего использования

---

## Дистанции и зоны

```cpp
struct Distances {
    float close = 500.0f;      // близкая зона
    float medium = 5000.0f;     // средняя зона
    float far = 20000.0f;       // дальняя зона
    float horizon = 40000.0f;   // горизонт
} dist;

glm::vec3 hullColorNear;  // цвет в близкой зоне
glm::vec3 hullColorMid;   // цвет в средней зоне
glm::vec3 hullColorFar;    // цвет в дальней зоне
```

### 📌 Как работают дистанции
В шейдере `large_object_fill` цвет корпуса меняется в зависимости от расстояния до камеры:

```glsl
if (dist < distClose) {
    hullColor = hullColorNear;           // близко - темный
} else if (dist < distMedium) {
    // Плавный переход
    float t = (dist - distClose) / (distMedium - distClose);
    hullColor = mix(hullColorNear, hullColorMid, t);
} else if (dist < distFar) {
    // Плавный переход
    float t = (dist - distMedium) / (distFar - distMedium);
    hullColor = mix(hullColorMid, hullColorFar, t);
} else {
    // Дальняя зона
    float t = min((dist - distFar) / (distHorizon - distFar), 1.0);
    hullColor = mix(hullColorFar, hullColorFar * 1.5, t);
}
```

### 📌 Алгоритм работы
1. **Близко** (< close) - используем `hullColorNear`
2. **Средняя зона** (close - medium) - плавный переход от Near к Mid
3. **Далеко** (medium - far) - плавный переход от Mid к Far
4. **Горизонт** (> far) - плавное осветление до горизонта

---

## Эффекты поверхности

```cpp
float fresnelPower = 2.0f;     // степень френеля
float rimIntensity = 0.5f;      // интенсивность подсветки краев
float edgeIntensity = 0.3f;     // интенсивность ребер (не используется)
float normalBlend = 0.15f;      // смешивание с цветом нормалей
```

### 📌 fresnelPower
- **Что это**: Степень эффекта Френеля (подсветка краев)
- **Диапазон**: 1.0 - 10.0
- **Как работает**: Чем выше значение, тем уже полоса подсветки на краях

### 📌 rimIntensity
- **Что это**: Интенсивность подсветки краев
- **Диапазон**: 0.0 - 1.0
- **Как работает**: Умножается на результат Френеля

### 📌 normalBlend
- **Что это**: Смешивание с "цветом нормалей" (визуализация нормалей)
- **Диапазон**: 0.0 - 1.0
- **Как работает**: 0 - только освещение, 1 - только нормали

---

## Сетка (Grid)

```cpp
struct Grid {
    bool enabled = false;           // включена ли сетка
    float cellSize = 100.0f;         // размер ячейки
    float lineWidth = 1.0f;          // толщина линий
    glm::vec3 lineColor = glm::vec3(0.93f, 0.93f, 0.94f); // цвет линий
    float lineAlpha = 0.3f;          // прозрачность линий
    float glow = 0.2f;                // свечение линий
    glm::vec3 cellSizeXYZ = glm::vec3(100.0f); // размер по осям
    glm::vec3 offset = glm::vec3(0.0f);         // смещение
} grid;
```

### 📌 Как работает сетка
Сетка проецируется на поверхность объекта в локальных координатах:

1. **Планарное проецирование**: Сетка накладывается на поверхность
2. **Адаптивная толщина**: Толщина линий меняется с расстоянием
3. **Свечение**: Линии могут светиться

### 📌 Алгоритм сетки
```glsl
// 1. Локальные координаты с учетом смещения
vec3 localPos = vObjectPos + gridOffset;

// 2. Построение базиса для проецирования
vec3 tangent = normalize(cross(n, vec3(0.0, 1.0, 0.0)));
vec3 bitangent = normalize(cross(n, tangent));

// 3. 2D координаты на поверхности
vec2 planarPos = vec2(dot(localPos, tangent), dot(localPos, bitangent));

// 4. Расчет расстояния до линий сетки
vec2 gridCoords = planarPos / vec2(cellSize.x, cellSize.y);
vec2 distToLine = min(fract(gridCoords), 1.0 - fract(gridCoords)) * cellSize;
```

---

## Ребра (Edge)

```cpp
glm::vec3 edgeColor = glm::vec3(1.0f, 1.0f, 1.0f);  // цвет ребер
float edgeAlpha = 1.0f;                              // прозрачность
float edgeThickness = 1.0f;                          // толщина в пикселях
float edgeFadeStart = 500.0f;                        // начало затухания
float edgeFadeEnd = 5000.0f;                          // конец затухания
```

### 📌 edgeColor
- **Что это**: Цвет ребер (линий)
- **Формат**: (R, G, B) от 0.0 до 1.0

### 📌 edgeAlpha
- **Что это**: Прозрачность ребер
- **Диапазон**: 0.0 (прозрачные) - 1.0 (непрозрачные)

### 📌 edgeThickness
- **Что это**: Толщина линий в пикселях
- **Диапазон**: 0.5 - 5.0 (обычно 1.0-2.0)

### 📌 edgeFadeStart / edgeFadeEnd
- **Что это**: Дистанции затухания ребер
- **Алгоритм**: 
  - До `fadeStart` - ребра видны полностью
  - От `fadeStart` до `fadeEnd` - плавно исчезают
  - После `fadeEnd` - не видны

---

## Туман (Fog)

```cpp
struct Fog {
    bool enabled = true;                    // включен ли туман
    float startDistance = 300.0f;            // начало тумана
    float endDistance = 3000.0f;              // конец тумана
    glm::vec3 nearColor = glm::vec3(0.05f, 0.05f, 0.1f);  // цвет вблизи
    glm::vec3 farColor = glm::vec3(0.0f, 0.0f, 0.02f);     // цвет вдали
    float intensity = 0.5f;                   // интенсивность
} fog;
```

### 📌 Алгоритм тумана
```glsl
// Расстояние до объекта
float dist = length(vWorldPos - cameraPos);

// Фактор тумана (0 - нет тумана, 1 - полный туман)
float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
fogFactor *= fogIntensity;

// Цвет тумана с переходом
vec3 fogColor = mix(fogNearColor, fogFarColor, fogFactor);

// Смешивание цвета объекта с туманом
finalColor = mix(finalColor, fogColor, fogFactor);
```

### 📌 Параметры тумана
- **startDistance**: Расстояние, с которого начинается туман
- **endDistance**: Расстояние, на котором туман полностью скрывает объект
- **nearColor**: Цвет тумана вблизи (обычно темный)
- **farColor**: Цвет тумана вдали (обычно черный)
- **intensity**: Общая интенсивность тумана (0-1)

---

## Пресеты

### station()
```cpp
static LightingParams station() {
    LightingParams p;
    
    // Яркое освещение
    p.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    p.ambientColor = glm::vec3(1.0f, 0.3f, 0.35f);  // красноватый амбиент
    
    // Большие дистанции (станции большие)
    p.dist.close = 500.0f;
    p.dist.medium = 5000.0f;
    p.dist.far = 100000.0f;
    p.dist.horizon = 200000.0f;
    
    // Цвета корпуса по зонам
    p.hullColorNear = glm::vec3(0.1f, 0.1f, 0.14f);
    p.hullColorMid = glm::vec3(0.3f, 0.3f, 0.35f);
    p.hullColorFar = glm::vec3(0.5f, 0.5f, 0.6f);
    
    // Ребра видны далеко
    p.edgeFadeStart = 500.0f;
    p.edgeFadeEnd = 7000.0f;
    
    // Туман на больших расстояниях
    p.fog.startDistance = 100.0f;
    p.fog.endDistance = 60000.0f;
    
    return p;
}
```

### ship()
```cpp
static LightingParams ship() {
    LightingParams p;
    
    // Освещение
    p.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    p.ambientColor = glm::vec3(1.0f, 0.3f, 0.35f);  // легкий оттенок
    
    // Основной цвет
    p.hullColor = glm::vec3(0.04f, 0.04f, 0.08f);  // темно-синий
    
    // Ребра (ближе к кораблю)
    p.edgeColor = glm::vec3(0.50f, 0.5f, 0.55f);
    p.edgeFadeStart = 100.0f;
    p.edgeFadeEnd = 1000.0f;
    
    // Эффекты поверхности
    p.fresnelPower = 2.0f;
    p.rimIntensity = 0.5f;
    p.normalBlend = 0.15f;
    
    // Туман
    p.fog.startDistance = 100.0f;
    p.fog.endDistance = 3000.0f;
    
    return p;
}
```

### asteroid()
```cpp
static LightingParams asteroid() {
    LightingParams p;
    
    // Маленькие дистанции (астероиды мелкие)
    p.dist.close = 300.0f;
    p.dist.medium = 3000.0f;
    p.dist.far = 10000.0f;
    
    // Землистые цвета
    p.hullColorNear = glm::vec3(0.5f, 0.4f, 0.3f);
    p.hullColorMid = glm::vec3(0.6f, 0.5f, 0.4f);
    p.hullColorFar = glm::vec3(0.7f, 0.6f, 0.5f);
    
    // Сетка включена
    p.grid.enabled = true;
    p.grid.lineColor = glm::vec3(0.6f, 0.5f, 0.4f);
    
    return p;
}
```

### planet()
```cpp
static LightingParams planet() {
    LightingParams p;
    
    // Огромные дистанции
    p.dist.close = 2000.0f;
    p.dist.medium = 20000.0f;
    p.dist.far = 100000.0f;
    p.dist.horizon = 200000.0f;
    
    // Голубоватые тона
    p.hullColorNear = glm::vec3(0.2f, 0.3f, 0.6f);
    p.hullColorMid = glm::vec3(0.3f, 0.4f, 0.7f);
    p.hullColorFar = glm::vec3(0.4f, 0.5f, 0.8f);
    
    return p;
}
```

---

## Uniform Applicators

### applyLargeObjectUniforms()
**Для кого**: Станции, астероиды, планеты (шейдер `large_object_fill`)

```cpp
void applyLargeObjectUniforms(GLuint shader, const glm::mat4& mvp, 
                             const glm::mat4& model, const glm::vec3& cameraPos) const;
```

**Что передает**:
- Матрицы (MVP, M, normalMat)
- Позицию камеры
- Параметры освещения
- Дистанции и цвета по зонам
- Параметры сетки
- Параметры тумана

### applyShipUniforms()
**Для кого**: Корабли (шейдер `mesh_fill`)

```cpp
void applyShipUniforms(GLuint shader, const glm::mat4& mvp, 
                      const glm::mat4& model, const glm::vec3& cameraPos) const;
```

**Что передает**:
- Матрицы
- Позицию камеры
- Параметры освещения
- Цвет корпуса
- Эффекты поверхности (fresnel, rim, normalBlend)
- Параметры тумана

### applyEdgeUniforms()
**Для кого**: Ребра (шейдер `edge_shader`)

```cpp
void applyEdgeUniforms(GLuint shader, const glm::mat4& mvp, 
                      const glm::vec3& cameraPos,
                      int viewportWidth, int viewportHeight) const;
```

**Что передает**:
- Матрицу MVP
- Позицию камеры
- Размер вьюпорта (для толщины линий)
- Цвет, прозрачность, толщину ребер
- Параметры затухания

---

## Как работать с шейдерами

### 1. Инициализация шейдеров
```cpp
// В InitShaders.cpp
shaders.load(
    "mesh_fill",
    "assets/shaders/mesh/mesh_fill.vert",
    "assets/shaders/mesh/mesh_fill.frag",
    ShaderProgram::MeshFill  // ← указываем тип!
);
```

### 2. Получение шейдера
```cpp
GLuint fillShader = ShaderLibrary::instance().get("mesh_fill");
GLuint edgeShader = ShaderLibrary::instance().get("edge_shader");
```

### 3. Создание параметров
```cpp
LightingParams params = LightingParams::ship();  // или station(), planet()

// Можно изменить отдельные параметры
params.hullColor = glm::vec3(0.1f, 0.2f, 0.3f);
params.rimIntensity = 0.3f;
```

### 4. Рендеринг объекта
```cpp
m_meshRenderer.draw(
    *gpuMesh,           // меш объекта
    fillShader,         // шейдер для корпуса
    edgeShader,         // шейдер для ребер
    mvp,                // матрица MVP
    model,              // матрица модели
    params,             // параметры освещения
    cameraPos           // позиция камеры
);
```

### 5. В MeshRenderer.cpp автоматически определяется тип шейдера
```cpp
ShaderProgram shaderType = ShaderLibrary::instance().getType(meshShader);

if (shaderType == ShaderProgram::MeshFill) {
    params.applyShipUniforms(meshShader, mvp, model, cameraPos);
} else if (shaderType == ShaderProgram::LargeObjectFill) {
    params.applyLargeObjectUniforms(meshShader, mvp, model, cameraPos);
}
```

---

## Практические примеры

### Пример 1: Темный корабль с легкой подсветкой краев
```cpp
static LightingParams stealthShip() {
    LightingParams p;
    
    // Очень темное освещение
    p.lightColor = glm::vec3(0.3f, 0.3f, 0.4f);
    p.ambientColor = glm::vec3(0.05f, 0.05f, 0.1f);
    
    // Черный корпус
    p.hullColor = glm::vec3(0.02f, 0.02f, 0.03f);
    
    // Едва заметные края
    p.rimIntensity = 0.1f;
    p.fresnelPower = 3.0f;
    
    // Тонкие темные ребра
    p.edgeColor = glm::vec3(0.2f, 0.2f, 0.25f);
    p.edgeAlpha = 0.5f;
    p.edgeFadeStart = 50.0f;
    p.edgeFadeEnd = 500.0f;
    
    return p;
}
```

### Пример 2: Яркая станция с сеткой
```cpp
static LightingParams researchStation() {
    LightingParams p = LightingParams::station();
    
    // Яркое освещение
    p.lightColor = glm::vec3(1.2f, 1.2f, 1.2f);
    
    // Белый корпус с голубым отливом
    p.hullColorNear = glm::vec3(0.9f, 0.95f, 1.0f);
    p.hullColorMid = glm::vec3(0.8f, 0.85f, 0.9f);
    p.hullColorFar = glm::vec3(0.7f, 0.75f, 0.8f);
    
    // Синие ребра
    p.edgeColor = glm::vec3(0.3f, 0.6f, 1.0f);
    
    // Сетка для техно-стиля
    p.grid.enabled = true;
    p.grid.lineColor = glm::vec3(0.2f, 0.5f, 1.0f);
    p.grid.lineAlpha = 0.5f;
    p.grid.glow = 0.3f;
    
    return p;
}
```

### Пример 3: Астероид с красным свечением
```cpp
static LightingParams redAsteroid() {
    LightingParams p = LightingParams::asteroid();
    
    // Красноватое освещение
    p.lightColor = glm::vec3(1.0f, 0.7f, 0.6f);
    p.ambientColor = glm::vec3(0.3f, 0.1f, 0.1f);
    
    // Красноватые тона
    p.hullColorNear = glm::vec3(0.7f, 0.3f, 0.2f);
    p.hullColorMid = glm::vec3(0.8f, 0.4f, 0.3f);
    p.hullColorFar = glm::vec3(0.9f, 0.5f, 0.4f);
    
    // Сетка с красным свечением
    p.grid.lineColor = glm::vec3(1.0f, 0.3f, 0.2f);
    p.grid.glow = 0.5f;
    
    return p;
}
```

---

## Важные замечания

### 🔥 Ключевые принципы
1. **Ambient должен быть слабым** (0.1-0.3), иначе тени будут "светиться"
2. **Rim и Specular** лучше умножать, а не складывать
3. **Дистанции** должны соответствовать размеру объекта
4. **Цвета в шейдере** всегда в диапазоне 0.0-1.0

### 🚫 Частые ошибки
1. Использование `ambientColor.r` вместо полного вектора
2. Слишком яркий ambient в тенях
3. Неправильные дистанции для размера объекта
4. Забытый тип шейдера при загрузке

### ✅ Правильный подход
1. Начинать с простого цвета (Блок 0)
2. Добавлять освещение поэтапно
3. Использовать отладочный вывод uniform'ов
4. Тестировать на разных расстояниях
