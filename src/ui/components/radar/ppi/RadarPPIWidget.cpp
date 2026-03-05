#include "RadarPPIWidget.h"
#include "effects/CRTEffects.h"
#include "effects/LCDEffects.h"
#include <glad/gl.h>
#include <algorithm>
#include <iostream>
#include <glm/gtc/constants.hpp>

void RadarPPIWidget::init(game::RadarVisualProfile type)
{
    
    m_shaderTime = 0.0f;
    m_shaderEnabled = false;
    m_postShader = 0;
    m_time = 0.0f;
    
    // Выбираем тип эффекта
    switchEffect(type);

    // if (auto* crt = dynamic_cast<CRTEffect*>(m_effect.get())) {
        // Постоянные эффекты должны быть > 0
        // Они уже установлены в CRTEffectsData со значениями по умолчанию:
        
        // Для теста включаем глюки
        // crt->enableTearing(true);
        // crt->enableEpisodicHumBars(true);
        // crt->enableGhosting(true);
        // crt->enableVerticalRoll(true);
    // }


    if (auto* lcd = dynamic_cast<LCDEffect*>(m_effect.get())) {
    // Включаем все с максимальной интенсивностью
    lcd->enableCableLines(false);
    // lcd->enableDigitalNoise(true);
    // lcd->enableTearing(true);
    // lcd->enableInterference(true);
    }
    // m_freezeTestEnabled = true;
    // lcd->enableFreeze(true);
    // lcd->setFreezeParams(0, 1.0f, 3.0f, 5.0f);
    // Режим 0: Полный фриз (всё застывает)
    // lcd->setFreezeParams(0, 0.9f, 3.0f, 5.0f);
    // Режим 1: Частичный фриз (только часть экрана)
    // lcd->setFreezeParams(1, 0.8f, 3.0f, 5.0f);
    // Режим 2: Полосами (застывает полосами)
    // lcd->setFreezeParams(2, 0.7f, 4.0f, 5.0f);
    // Режим 3: Мерцающий (то застывает, то нет)
    // lcd->setFreezeParams(3, 0.6f, 3.0f, 3.0f);
    // Можно также установить параметры
// }

}

void RadarPPIWidget::switchEffect(game::RadarVisualProfile type)
{
    switch(type) {
        case game::RadarVisualProfile::CRT:
            m_effect = std::make_unique<CRTEffect>();
            initShader("assets/shaders/hud/crt_effects.frag");
            break;
            
        case game::RadarVisualProfile::LCD:
            m_effect = std::make_unique<LCDEffect>();
            initShader("assets/shaders/hud/lcd_effects.frag");
            break;
            
        default:
            m_effect = std::make_unique<CRTEffect>();
            initShader("assets/shaders/hud/crt_effects.frag");
            break;
    }
}

void RadarPPIWidget::initShader(const std::string& fragPath)
{
    std::string vertexPath = "assets/shaders/hud/passthrough.vert";
    printf("Loading shader: %s and %s\n", vertexPath.c_str(), fragPath.c_str());   
    m_postShader = compileShaderFromFiles(vertexPath, fragPath);
    m_shaderEnabled = (m_postShader != 0);   
    printf("Shader load result: program=%d, enabled=%d\n", m_postShader, m_shaderEnabled);
}





void RadarPPIWidget::update(float dt)
{
    // Сначала вызываем базовую логику (обновление угла и т.д.)
    RadarWidgetBase::update(dt);  // ← ЭТО ВАЖНО!
    
    m_time += dt;  // обновляем локальное время
    
    // Синхронизация контактов (было в вашем коде)
    for (const auto& c : m_contacts)
    {
        auto it = m_ppiContacts.find(c.id);
        if (it == m_ppiContacts.end())
        {
            PPIContact ppi;
            ppi.data = c;
            m_ppiContacts.emplace(c.id, ppi);
        }
        else
        {
            it->second.data = c;
        }
    }

    // Sweep detection (было в вашем коде)
    for (auto& pair : m_ppiContacts)
    {
        PPIContact& contact = pair.second;
        const glm::vec3& local = contact.data.Position;
        float horizontalDist = std::sqrt(local.x * local.x + local.z * local.z);

        if (horizontalDist > m_displayRange)
            continue;

        float azimuth = glm::degrees(std::atan2(local.x, local.z));
        if (azimuth < 0.0f)
            azimuth += 360.0f;

        float diff = std::abs(azimuth - m_sweepAngle);
        if (diff > 180.0f)
            diff = 360.0f - diff;

        if (diff < m_sweepWidthDeg)
        {
            contact.lastHitTime = m_time;
            contact.latchedLocal = local;
        }
    }

    // Удаляем исчезнувшие цели
    for (auto it = m_ppiContacts.begin(); it != m_ppiContacts.end(); )
    {
        bool exists = std::any_of(m_contacts.begin(), m_contacts.end(),
            [&](const RadarContactView& c) { return c.id == it->first; });

        if (!exists)
            it = m_ppiContacts.erase(it);
        else
            ++it;
    }

    // ===== ВАЖНО: ОБНОВЛЯЕМ ЭФФЕКТЫ =====
    if (m_effect) {
        m_effect->update(dt);
    }
}






void RadarPPIWidget::applyVisualConfig(const IRadarVisualConfig& base)
{
    const PPIVisualConfig* cfg = dynamic_cast<const PPIVisualConfig*>(&base);
    if (!cfg)
        throw std::runtime_error("Wrong visual config type for RadarPPIWidget");

    // Заполняем настройки внешнего вида
    m_visualConfig.background   = cfg->background;
    m_visualConfig.sweep        = cfg->sweep;
    m_visualConfig.contact      = cfg->contact;
    m_visualConfig.graticule    = cfg->graticule;


    // Загружаем текстуры если есть
    if (cfg->backgroundTexturePath)
        m_bg = TextureLoader::load2D(cfg->backgroundTexturePath);
    if (cfg->overlayTexturePath)
        m_overlay = TextureLoader::load2D(cfg->overlayTexturePath);
}


void RadarPPIWidget::applyEffectsConfig(const game::IRadarEffectsConfig& effects){
    if (!m_effect) return;
    
    if (!m_effect) return;
    
    // Пытаемся привести к CRT конфигу
    auto crtConfig = dynamic_cast<const game::PPICRTEffectsConfig*>(&effects);
    if (crtConfig) {
        auto ptr = std::make_shared<game::PPICRTEffectsConfig>(*crtConfig);
        m_effect->loadConfig(ptr);
        return;
    }
    
    // Пытаемся привести к LCD конфигу
    auto lcdConfig = dynamic_cast<const game::PPILCDEffectsConfig*>(&effects);
    if (lcdConfig) {
        auto ptr = std::make_shared<game::PPILCDEffectsConfig>(*lcdConfig);
        m_effect->loadConfig(ptr);
        return;
    }

}








void RadarPPIWidget::renderBackground()
{
    glColor4f(m_visualConfig.background.color.r, 
              m_visualConfig.background.color.g, 
              m_visualConfig.background.color.b, 
              m_visualConfig.background.color.a);
              
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(m_centerX, m_centerY);
    for (int i = 0; i <= 360; ++i)
    {
        float a = glm::radians((float)i);
        float x = m_centerX + std::cos(a) * m_radius;
        float y = m_centerY + std::sin(a) * m_radius * m_visualConfig.background.perspective;
        glVertex2f(x, y);
    }
    glEnd();
}

void RadarPPIWidget::renderOverlay()
{
    renderGraticule();
}






void RadarPPIWidget::renderRadarContent(float px, float py, float pw, float ph)
{
    if (pw <= 0.0f || ph <= 0.0f)
        return;

    // Сохраняем состояние OpenGL
    GLint previousFBO;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
    GLint previousViewport[4];
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    GLboolean previousBlend = glIsEnabled(GL_BLEND);
    GLboolean previousDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean previousTexture2D = glIsEnabled(GL_TEXTURE_2D);

    // Создаём FBO
    int fboW = std::max(1, std::min(2048, static_cast<int>(pw)));
    int fboH = std::max(1, std::min(2048, static_cast<int>(ph)));
    m_renderTarget.ensureSize(fboW, fboH);

    // =========================================================================
    // 1. РЕНДЕР В FBO (текущий кадр) - ВСЕГДА РАБОТАЕТ
    // ========================================================================= 
    m_renderTarget.bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, fboW, fboH, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    m_centerX = fboW * 0.5f;
    m_centerY = fboH * 0.5f;
    m_radius = std::max(fboW, fboH) * 0.45f;

    // Обновляем время шейдера
    static float lastTime = 0.0f;
    float dt = m_time - lastTime;
    if (dt > 0.1f) dt = 1.0f/60.0f;
    m_shaderTime += dt;
    lastTime = m_time;

    // Рисуем компоненты
    renderBackground();
    renderGraticule();
    renderContacts();
    renderSweep();

    // Восстанавливаем матрицы
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();



    // =========================================================================
    // 2. ЭКСПЕРИМЕНТАЛЬНАЯ СЕКЦИЯ - FREEZE ТЕСТ (может быть удалена в любой момент)
    // =========================================================================
    if (m_freezeTestEnabled) {
        // Инициализируем тестовый FBO если нужно
        m_freezeTestTarget.ensureSize(fboW, fboH);

               
        // Сохраняем текущий кадр в тестовый FBO
        m_freezeTestTarget.bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Рисуем текущую текстуру в тестовый FBO
        glBindTexture(GL_TEXTURE_2D, m_renderTarget.texture());
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex2f(0, 0);
            glTexCoord2f(1, 0); glVertex2f(fboW, 0);
            glTexCoord2f(1, 1); glVertex2f(fboW, fboH);
            glTexCoord2f(0, 1); glVertex2f(0, fboH);
        glEnd();
        
        m_freezeTestTarget.unbind();
    }
    
    // Отвязываем основной FBO
    m_renderTarget.unbind();
    
    // =========================================================================
    // 3. ВЫВОД НА ЭКРАН - ВСЕГДА РАБОТАЕТ
    // =========================================================================
    // Вывод на экран с шейдером
    glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
    glViewport(previousViewport[0], previousViewport[1], 
               previousViewport[2], previousViewport[3]);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, previousViewport[2], previousViewport[3], 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    // Применяем шейдер
    if (m_shaderEnabled && m_postShader && m_effect) {
        glUseProgram(m_postShader);
        
        // Базовые параметры для всех шейдеров
        glUniform1f(glGetUniformLocation(m_postShader, "time"), m_shaderTime);
        glUniform2f(glGetUniformLocation(m_postShader, "resolution"), fboW, fboH);
        
        // Параметры эллипса для маски
        glUniform1f(glGetUniformLocation(m_postShader, "radarCenterX"), m_centerX);
        glUniform1f(glGetUniformLocation(m_postShader, "radarCenterY"), m_centerY);
        glUniform1f(glGetUniformLocation(m_postShader, "radarRadiusX"), m_radius);
        glUniform1f(glGetUniformLocation(m_postShader, "radarRadiusY"), 
                   m_radius * m_visualConfig.background.perspective);
        
        // Специфичные для эффекта параметры
        m_effect->applyToShader(m_postShader, m_shaderTime,
                                m_centerX, m_centerY,
                                m_radius, m_radius * m_visualConfig.background.perspective);
    }

    // Рисуем текстуру
    glBindTexture(GL_TEXTURE_2D, m_renderTarget.texture());
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(px,      py);
        glTexCoord2f(1, 0); glVertex2f(px + pw, py);
        glTexCoord2f(1, 1); glVertex2f(px + pw, py + ph);
        glTexCoord2f(0, 1); glVertex2f(px,      py + ph);
    glEnd();

    if (m_shaderEnabled && m_postShader)
        glUseProgram(0);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Восстанавливаем состояние OpenGL
    if (!previousBlend) glDisable(GL_BLEND);
    if (previousDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (!previousTexture2D) glDisable(GL_TEXTURE_2D);
}






void RadarPPIWidget::renderSweep()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(m_visualConfig.sweep.lineWidth);

    for (int step = 0; step < m_visualConfig.sweep.trailSteps; ++step)
    {
        float t = (float)step / (m_visualConfig.sweep.trailSteps - 1);
        float angleDeg = m_sweepAngle - t * m_visualConfig.sweep.trailLengthDeg;
        float angleRad = glm::radians(angleDeg);
        float trailIntensity = std::pow(1.0f - t, m_visualConfig.sweep.trailFadePower);

        glColor4f(m_visualConfig.sweep.color.r, 
                  m_visualConfig.sweep.color.g * trailIntensity,
                  m_visualConfig.sweep.color.b, 
                  m_visualConfig.sweep.color.a);

        float dirX = std::sin(angleRad);
        float dirY = std::cos(angleRad);
        float x = m_centerX + dirX * m_radius;
        float y = m_centerY - dirY * m_radius * m_visualConfig.background.perspective;

        glBegin(GL_LINES);
        glVertex2f(m_centerX, m_centerY);
        glVertex2f(x, y);
        glEnd();
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

void RadarPPIWidget::renderContacts()
{
    if (m_sweepSpeed <= 0.0f)
        return;

    const float usableRadius = m_radius * m_visualConfig.contact.radiusScale;
    const float rotationPeriod = 360.0f / m_sweepSpeed;
    const float fadeDuration = rotationPeriod * 0.9f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& pair : m_ppiContacts)
    {
        const PPIContact& contact = pair.second;
        float age = m_time - contact.lastHitTime;
        float intensity = 1.0f - (age / fadeDuration);

        if (intensity <= 0.0f)
            continue;

        intensity = std::clamp(intensity, 0.0f, 1.0f);
        glm::vec4 finalColor = m_visualConfig.contact.color;

        const glm::vec3& local = contact.latchedLocal;
        float horizontalDist = std::sqrt(local.x * local.x + local.z * local.z);

        if (horizontalDist > m_displayRange)
            continue;

        float radial = horizontalDist / m_displayRange;
        float azimuth = std::atan2(local.x, local.z);

        float baseX = m_centerX + std::sin(azimuth) * radial * usableRadius;
        float baseY = m_centerY - std::cos(azimuth) * radial * usableRadius * m_visualConfig.background.perspective;

        float heightNorm = local.y / m_displayRange;
        heightNorm = std::clamp(heightNorm, -1.0f, 1.0f);
        float heightOffset = heightNorm * usableRadius * m_visualConfig.contact.verticalScale;
        float finalY = baseY + heightOffset;

        glColor4f(finalColor.r, finalColor.g, finalColor.b, finalColor.a);

        // Горизонтальная линия
        glBegin(GL_LINES);
        glVertex2f(baseX - m_visualConfig.contact.halfWidth, baseY);
        glVertex2f(baseX + m_visualConfig.contact.halfWidth, baseY);
        glEnd();

        // Вертикальная линия
        glBegin(GL_LINES);
        glVertex2f(baseX, baseY);
        glVertex2f(baseX, finalY);
        glEnd();

        // Квадратик
        glBegin(GL_LINE_LOOP);
        glVertex2f(baseX - m_visualConfig.contact.boxSize, finalY - m_visualConfig.contact.boxSize);
        glVertex2f(baseX + m_visualConfig.contact.boxSize, finalY - m_visualConfig.contact.boxSize);
        glVertex2f(baseX + m_visualConfig.contact.boxSize, finalY + m_visualConfig.contact.boxSize);
        glVertex2f(baseX - m_visualConfig.contact.boxSize, finalY + m_visualConfig.contact.boxSize);
        glEnd();
    }

    glDisable(GL_BLEND);
}

void RadarPPIWidget::renderGraticule()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(m_visualConfig.graticule.lineWidth);
    
    // Базовый цвет сетки (зеленый)
    // const glm::vec4 BASE_COLOR = {0.0f, 1.0f, 0.0f, 1.0f};
    const glm::vec4 BASE_COLOR = m_visualConfig.graticule.color;
    float intensity = m_visualConfig.graticule.intensity;
    
    // Если интенсивность меньше порога - вообще не рисуем
    if (intensity <= 0.01f) {
        glDisable(GL_BLEND);
        return;
    }
    
    // ===== КОНЦЕНТРИЧЕСКИЕ ОКРУЖНОСТИ =====
    for (int r = 1; r <= m_visualConfig.graticule.numRings; r++) {
        float ringRadius = m_radius * (r / (float)m_visualConfig.graticule.numRings);
        
        // Дальние кольца чуть темнее (но не слишком)
        float ringFactor = 1.0f - (r - 1) * 0.1f;
        float finalIntensity = intensity * ringFactor;
        
        // Яркость RGB и прозрачность зависят от intensity
        glColor4f(BASE_COLOR.r * finalIntensity, 
                  BASE_COLOR.g * finalIntensity, 
                  BASE_COLOR.b * finalIntensity, 
                  finalIntensity);  // Альфа = той же интенсивности
        
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= 360; i += 5) {
            float a = glm::radians((float)i);
            float x = m_centerX + std::cos(a) * ringRadius;
            float y = m_centerY + std::sin(a) * ringRadius * m_visualConfig.background.perspective;
            glVertex2f(x, y);
        }
        glEnd();
    }
    
    // ===== РАДИАЛЬНЫЕ ЛИНИИ =====
    for (int i = 0; i < m_visualConfig.graticule.numRays; i++) {
        float angle = glm::radians(i * (360.0f / m_visualConfig.graticule.numRays));
        float x = m_centerX + std::cos(angle) * m_radius;
        float y = m_centerY + std::sin(angle) * m_radius * m_visualConfig.background.perspective;
        
        // Радиальные линии чуть темнее колец
        float radialIntensity = intensity * 0.8f;
        
        glColor4f(BASE_COLOR.r * radialIntensity, 
                  BASE_COLOR.g * radialIntensity, 
                  BASE_COLOR.b * radialIntensity, 
                  radialIntensity);
        
        glBegin(GL_LINES);
        glVertex2f(m_centerX, m_centerY);
        glVertex2f(x, y);
        glEnd();
    }
    
    // ===== МЕТКИ КУРСА (0°, 90°, 180°, 270°) =====
    float cardinals[] = {0, 90, 180, 270};
    for (float deg : cardinals) {
        float angle = glm::radians(deg);
        float x = m_centerX + std::cos(angle) * m_radius;
        float y = m_centerY + std::sin(angle) * m_radius * m_visualConfig.background.perspective;
        
        // Кардинальные направления ярче
        float cardinalIntensity = intensity * 1.2f;
        
        glColor4f(BASE_COLOR.r * cardinalIntensity, 
                  BASE_COLOR.g * cardinalIntensity, 
                  BASE_COLOR.b * cardinalIntensity, 
                  cardinalIntensity);
        
        glBegin(GL_LINES);
        glVertex2f(m_centerX, m_centerY);
        glVertex2f(x, y);
        glEnd();
    }
    
    glDisable(GL_BLEND);
}


void RadarPPIWidget::setVisualConfig(const PPIVisualConfig& config)
{
    m_visualConfig = config;
}