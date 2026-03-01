#version 120

// ============================================================================
// ВХОДНЫЕ ПАРАМЕТРЫ ШЕЙДЕРА
// ============================================================================

// Основные текстуры и параметры
uniform sampler2D texture;      // Текстура с рендером радара
uniform float time;             // Время для анимации эффектов
uniform vec2 resolution;        // Размер текстуры (width, height)

// Параметры формы радара (передаются из C++)
uniform float radarCenterX;      // Центр радара по X (в пикселях)
uniform float radarCenterY;      // Центр радара по Y (в пикселях)
uniform float radarRadiusX;      // Радиус по X (горизонтальный)
uniform float radarRadiusY;      // Радиус по Y (вертикальный, с учетом perspective)

// ----------------------------------------------------------------------------
// ПАРАМЕТРЫ ЭПИЗОДИЧЕСКИХ ГЛЮКОВ
// ----------------------------------------------------------------------------

// Tearing (рваное изображение)
uniform float tearIntensity;        // 0 = нет, >0 = активен
uniform float tearStripCount;       // Количество дергающихся полос
uniform float tearStripHeight;      // Толщина полос
uniform float tearMaxShift;         // Максимальный сдвиг в пикселях
uniform float tearChaosSpeed;       // Скорость дерганья

// Ghosting (двойное изображение)
uniform float ghostIntensity;       // 0 = нет, >0 = активен
uniform float ghostPrimaryOffset;   // Смещение первого призрака
uniform float ghostSecondaryOffset; // Смещение второго
uniform float ghostTertiaryOffset;  // Смещение третьего
uniform float ghostPrimaryIntensity;    // Яркость первого
uniform float ghostSecondaryIntensity;  // Яркость второго
uniform float ghostTertiaryIntensity;   // Яркость третьего

// Vertical Roll (проскок кадров)
uniform float rollIntensity;        // 0 = нет, >0 = активен
uniform float rollSpeed;            // Скорость прокрутки

// Episodic Hum Bars (эпизодические темные полосы)
uniform float humBarIntensity;      // 0 = нет, >0 = активен
uniform float humBarSpeed;          // Скорость движения полосы
uniform float humBarWidth;          // Ширина полосы

// ----------------------------------------------------------------------------
// ПОСТОЯННЫЕ ЭФФЕКТЫ (всегда активны)
// ----------------------------------------------------------------------------
uniform float flickerIntensity;             // Мерцание
uniform float flickerFrequency;             // Частота мерцания (было жестко 50.0)
uniform float breathingIntensity;           // Дыхание
uniform float breathingSpeed;               // Скорость дыхания (было жестко 2.0)
uniform float constantHumBarIntensity;      // Постоянные полосы
uniform float constantHumBarPhase;          // Фаза постоянных полос
uniform float constantHumBarWidth;          // Ширина постоянных полос

// ============================================================================
// ФУНКЦИЯ ПРОВЕРКИ, НАХОДИТСЯ ЛИ ТОЧКА ВНУТРИ ЭЛЛИПСА
// ============================================================================
bool isInsideEllipse(vec2 uv, vec2 center, vec2 radius)
{
    // Переводим UV-координаты (0-1) в пиксельные координаты
    vec2 pixelCoord = uv * resolution;
    
    // Нормализованные координаты относительно центра
    vec2 normalized = (pixelCoord - center) / radius;
    
    // Проверка: (x/rx)^2 + (y/ry)^2 <= 1
    return (normalized.x * normalized.x + normalized.y * normalized.y) <= 1.0;
}

// ============================================================================
// ОСНОВНАЯ ФУНКЦИЯ ШЕЙДЕРА
// ============================================================================
void main()
{
    // Текущие координаты текстуры (0-1)
    vec2 uv = gl_TexCoord[0].xy;
    // Сохраняем оригинальные координаты для эффектов, которым нужен исходный пиксель
    vec2 originalUv = uv;
    
    // Центр радара в пикселях
    vec2 center = vec2(radarCenterX, radarCenterY);
    vec2 radius = vec2(radarRadiusX, radarRadiusY);
    
    // Проверяем, внутри ли мы эллипса
    bool inside = isInsideEllipse(uv, center, radius);
    

    // ==========================================================================
    // 1. ПРОСКОК КАДРОВ (VERTICAL ROLL) - ПРОДВИНУТАЯ ВЕРСИЯ
    // ==========================================================================
    if (rollIntensity > 0.0 && inside) {
        // Генерируем псевдослучайный режим на основе времени
        // Меняется каждые 2-5 секунд
        float modeSeed = floor(time * 0.3);
        int mode = int(mod(modeSeed, 6.0));  // 0-5 режимов
        
        // Используем синус от modeSeed для "случайности"
        float random1 = sin(modeSeed * 10.0) * 0.5 + 0.5;
        float random2 = cos(modeSeed * 15.0) * 0.5 + 0.5;
        
        float speed = rollSpeed;
        float jitter = 0.0;
        
        if (mode == 0) {
            // Режим "КЛАССИКА" - равномерная прокрутка
            speed = 80.0;
            jitter = 0.0;
        }
        else if (mode == 1) {
            // Режим "МЕДЛЕННЫЙ" - еле ползет
            speed = 30.0;
            jitter = 0.1 * sin(time * 3.0);
        }
        else if (mode == 2) {
            // Режим "БЫСТРЫЙ" - мелькает
            speed = 200.0 + 50.0 * sin(time * 2.0);
            jitter = 0.2 * sin(time * 10.0);
        }
        else if (mode == 3) {
            // Режим "РВАНЫЙ" - дергается рывками
            speed = 150.0;
            // Скачки каждые 0.3 секунды
            jitter = floor(time * 3.0) * 0.3;
        }
        else if (mode == 4) {
            // Режим "ЗАМИРАНИЕ" - то стоп, то быстро
            if (fract(time * 0.5) < 0.3) {
                speed = 0.0;  // Замерло
            } else {
                speed = 300.0; // Рвануло
            }
        }
        else {
            // Режим "ХАОС" - все сразу
            speed = 50.0 + 300.0 * random1;
            jitter = random2 * sin(time * 20.0);
        }
        
        // Добавляем микродерганье для реализма
        float microJitter = 0.1 * sin(time * 100.0) * sin(time * 73.0);
        
        // Финальный сдвиг
        float rollOffset = fract(time * speed * rollIntensity * 0.01 + jitter + microJitter);
        uv.y = fract(uv.y + rollOffset);
    }
    
    // ==========================================================================
    // 2. РВАНОЕ ИЗОБРАЖЕНИЕ (TEARING) - ДЛЯ ВСЕГО ЭКРАНА
    // ==========================================================================
    if (tearIntensity > 0.0) {
        float shift = 0.0;
        
        // Используем tearStripCount для создания нужного количества полос
        // Ограничим разумным числом (максимум 5 для производительности)
        int stripCount = int(min(tearStripCount, 5.0));
        
        for (int i = 0; i < stripCount; i++) {
            float offset = float(i) * 0.25; // равномерно распределяем по высоте
            float stripPos = 0.2 + offset + 0.1 * sin(time * 0.5 + float(i));
            
            if (uv.y > stripPos - tearStripHeight && uv.y < stripPos + tearStripHeight) {
                float chaos = sin(time * tearChaosSpeed + float(i)) * 0.7 + 
                             0.3 * sin(time * tearChaosSpeed * 0.7 + float(i) * 2.0);
                shift = tearMaxShift * tearIntensity * (0.5 + 0.5 * chaos);
                break; // только одна полоса за раз
            }
        }
        
        uv.x += shift / resolution.x;
    }
    
    // ==========================================================================
    // ПОЛУЧАЕМ ЦВЕТ ИЗ ТЕКСТУРЫ
    // ==========================================================================
    vec4 color = texture2D(texture, uv);
    
    // ==========================================================================
    // 3. ДВОЙНОЕ ИЗОБРАЖЕНИЕ (GHOSTING) - ДЛЯ ВСЕГО ЭКРАНА
    // ==========================================================================
    if (ghostIntensity > 0.0) {
        vec2 ghostUv1 = originalUv;
        ghostUv1.x += ghostPrimaryOffset * ghostIntensity / resolution.x;
        vec4 ghost1 = texture2D(texture, ghostUv1);
        
        vec2 ghostUv2 = originalUv;
        ghostUv2.x += ghostSecondaryOffset * ghostIntensity / resolution.x;
        vec4 ghost2 = texture2D(texture, ghostUv2);
        
        vec2 ghostUv3 = originalUv;
        ghostUv3.x += ghostTertiaryOffset * ghostIntensity / resolution.x;
        vec4 ghost3 = texture2D(texture, ghostUv3);
        
        color += ghost1 * ghostPrimaryIntensity * ghostIntensity;
        color += ghost2 * ghostSecondaryIntensity * ghostIntensity;
        color += ghost3 * ghostTertiaryIntensity * ghostIntensity;
        color = min(color, 1.0);
    }
    
    // ==========================================================================
    // 4. ЭПИЗОДИЧЕСКИЕ ТЕМНЫЕ ПОЛОСЫ - ДЛЯ ВСЕГО ЭКРАНА
    // ==========================================================================
    if (humBarIntensity > 0.0) {
        float barPos = fract(time * humBarSpeed);
        float dist = abs(uv.y - barPos);
        float darken = smoothstep(humBarWidth, 0.0, dist) * humBarIntensity;
        color.rgb *= (1.0 - darken);
    }
    
    // ==========================================================================
    // 5. ПОСТОЯННЫЕ ЭФФЕКТЫ - ДЛЯ ВСЕГО ЭКРАНА
    // ==========================================================================
    
    if (flickerIntensity > 0.0) {
        // Используем flickerFrequency из uniform
        float flicker = 1.0 - flickerIntensity * 
                        (0.3 + 0.3 * sin(time * flickerFrequency));
        color.rgb *= flicker;
    }
    
    if (breathingIntensity > 0.0) {
        // Используем breathingSpeed из uniform
        float breathe = 1.0 - breathingIntensity * 
                        (0.2 + 0.2 * sin(time * breathingSpeed));
        color.rgb *= breathe;
    }
    
    if (constantHumBarIntensity > 0.0) {
        float barPos = fract(constantHumBarPhase * 0.01);
        float dist = abs(uv.y - barPos);
        float darken = smoothstep(constantHumBarWidth, 0.0, dist) * constantHumBarIntensity;
        color.rgb *= (1.0 - darken);
    }
    
    // ==========================================================================
    // МАСКА - ВСЕ, ЧТО ВНЕ ЭЛЛИПСА, ДЕЛАЕМ ЧЕРНЫМ
    // ==========================================================================
    if (!inside) {
        color = vec4(0.0, 0.0, 0.0, 0.0);  // Полностью прозрачный черный
    }
    
    // ==========================================================================
    // ВЫВОД РЕЗУЛЬТАТА
    // ==========================================================================
    gl_FragColor = color;
}