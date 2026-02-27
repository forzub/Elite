#include "RadarCRTWidget.h"
#include <glad/gl.h>
#include <algorithm>
#include <iostream>
#include <glm/gtc/constants.hpp>
#include "src/game/equipment/radar/IRadarVisualConfig.h"
#include "src/game/equipment/radar/CRTVisualConfig.h"

void RadarCRTWidget::init(
    const char* bgPath,
    const char* overlayPath)
{
    m_bg = TextureLoader::load2D(bgPath);
    m_overlay = TextureLoader::load2D(overlayPath);
}




void RadarCRTWidget::update(float dt)
{
    m_time += dt;

    m_sweepAngle += m_sweepSpeed * dt;
    if (m_sweepAngle >= 360.0f)
        m_sweepAngle -= 360.0f;

    // --- синхронизация контактов ---
    for (const auto& c : m_contacts)
    {
        auto it = m_crtContacts.find(c.id);

        if (it == m_crtContacts.end())
        {
            CRTContact crt;
            crt.data = c;
            m_crtContacts.emplace(c.id, crt);
        }
        else
        {
            it->second.data = c;
        }
    }

    // --- sweep detection ---
    for (auto& pair : m_crtContacts)
    {
        CRTContact& crt = pair.second;

        const glm::vec3& local = crt.data.Position;

        float horizontalDist =
            std::sqrt(local.x * local.x + local.z * local.z);

        if (horizontalDist > m_displayRange)
            continue;

        float azimuth =
            glm::degrees(std::atan2(local.x, local.z));

        if (azimuth < 0.0f)
            azimuth += 360.0f;

        float diff = std::abs(azimuth - m_sweepAngle);
        if (diff > 180.0f)
            diff = 360.0f - diff;

        if (diff < m_sweepWidthDeg)
        {
            crt.lastHitTime = m_time;

            // 🔥 ВАЖНО: фиксируем позицию
            crt.latchedLocal = local;
        }
    }

    // --- удаляем исчезнувшие цели ---
    for (auto it = m_crtContacts.begin();
         it != m_crtContacts.end(); )
    {
        bool exists = std::any_of(
            m_contacts.begin(),
            m_contacts.end(),
            [&](const RadarContactView& c)
            {
                return c.id == it->first;
            });

        if (!exists)
            it = m_crtContacts.erase(it);
        else
            ++it;
    }
}


void RadarCRTWidget::renderBackground()
{
    glColor4f(m_bgColor.r, m_bgColor.g, m_bgColor.b, m_bgColor.a);

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(m_centerX, m_centerY);

    for (int i = 0; i <= 360; ++i)
    {
        float a = glm::radians((float)i);
        float x = m_centerX + std::cos(a) * m_radius;
        float y = m_centerY + std::sin(a) * m_radius * m_perspective;
        glVertex2f(x, y);
    }

    glEnd();


}











void RadarCRTWidget::renderOverlay()
{
    // временная заглушка
}





void RadarCRTWidget::renderSweep()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glDisable(GL_DEPTH_TEST);

    glLineWidth(m_sweepLineWidth);

    for (int step = 0; step < m_trailSteps; ++step)
    {
        float t = (float)step / (m_trailSteps - 1);

        float angleDeg = m_sweepAngle - t * m_trailLengthDeg;
        float angleRad = glm::radians(angleDeg);

        float intensity = std::pow(1.0f - t, m_trailFadePower);

        glColor4f(
            m_sweepColor.r,
            m_sweepColor.g * intensity,
            m_sweepColor.b,
            m_sweepColor.a * intensity
        );

        float x = m_centerX + std::sin(angleRad) * m_radius;
        float y = m_centerY + std::cos(angleRad) * m_radius * m_perspective;

        glBegin(GL_LINES);
        glVertex2f(m_centerX, m_centerY);
        glVertex2f(x, y);
        glEnd();
    }

    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}








void RadarCRTWidget::renderContacts()
{
    if (m_sweepSpeed <= 0.0f)
        return;

    const float usableRadius = m_radius * m_radiusScale;

    const float rotationPeriod = 360.0f / m_sweepSpeed;
    const float fadeDuration   = rotationPeriod * 0.9f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& pair : m_crtContacts)
    {
        const CRTContact& crt = pair.second;

        float age = m_time - crt.lastHitTime;
        float intensity = 1.0f - (age / fadeDuration);

        if (intensity <= 0.0f)
            continue;

        intensity = std::clamp(intensity, 0.0f, 1.0f);

        // 🔥 Используем зафиксированную позицию
        const glm::vec3& local = crt.latchedLocal;

        float horizontalDist =
            std::sqrt(local.x * local.x + local.z * local.z);

        if (horizontalDist > m_displayRange)
            continue;

        float radial = horizontalDist / m_displayRange;
        float azimuth = std::atan2(local.x, local.z);

        float baseX =
            m_centerX + std::sin(azimuth) *
            radial * usableRadius;

        float baseY =
            m_centerY + std::cos(azimuth) *
            radial * usableRadius * m_perspective;

        float heightNorm = local.y / m_displayRange;
        heightNorm = std::clamp(heightNorm, -1.0f, 1.0f);

        float heightOffset =
            heightNorm * usableRadius * m_verticalScale;

        float finalY = baseY - heightOffset;

        glColor4f(
            m_contactColor.r,
            m_contactColor.g * intensity,
            m_contactColor.b,
            m_contactColor.a * intensity
        );

        glBegin(GL_LINES);
        glVertex2f(baseX - m_contactHalfWidth, baseY);
        glVertex2f(baseX + m_contactHalfWidth, baseY);
        glEnd();

        glBegin(GL_LINES);
        glVertex2f(baseX, baseY);
        glVertex2f(baseX, finalY);
        glEnd();

        glBegin(GL_LINE_LOOP);
        glVertex2f(baseX - m_contactBoxSize, finalY - m_contactBoxSize);
        glVertex2f(baseX + m_contactBoxSize, finalY - m_contactBoxSize);
        glVertex2f(baseX + m_contactBoxSize, finalY + m_contactBoxSize);
        glVertex2f(baseX - m_contactBoxSize, finalY + m_contactBoxSize);
        glEnd();
    }

    glDisable(GL_BLEND);
}

void RadarCRTWidget::applyVisualConfig(
    const IRadarVisualConfig& base)
{
    const CRTVisualConfig* cfg =
        dynamic_cast<const CRTVisualConfig*>(&base);

    if (!cfg)
        throw std::runtime_error(
            "Wrong visual config type for RadarCRTWidget");

    // ===== Заполняем поля виджета =====

    m_bgColor        = cfg->m_bgColor;
    m_axisColor      = cfg->m_axisColor;

    m_sweepColor     = cfg->m_sweepColor;
    m_sweepLineWidth = cfg->m_sweepLineWidth;
    m_trailFadePower = cfg->m_trailFadePower;

    m_contactColor     = cfg->m_contactColor;
    m_contactHalfWidth = cfg->m_contactHalfWidth;
    m_contactBoxSize   = cfg->m_contactBoxSize;

    m_radiusScale   = cfg->m_radiusScale;
    m_verticalScale = cfg->m_verticalScale;

    if (cfg->backgroundTexturePath)
        m_bg = TextureLoader::load2D(
            cfg->backgroundTexturePath);

    if (cfg->overlayTexturePath)
        m_overlay = TextureLoader::load2D(
            cfg->overlayTexturePath);
}