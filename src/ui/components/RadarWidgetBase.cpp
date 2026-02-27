#include "RadarWidgetBase.h"
#include <glad/gl.h>
#include <cmath>
#include <iostream>

void RadarWidgetBase::update(float dt)
{
    if (m_sweepSpeed <= 0.0f)
        return; // например цифровой радар без sweep

    m_sweepAngle += dt * m_sweepSpeed;

    if (m_sweepAngle >= 360.0f)
        m_sweepAngle = std::fmod(m_sweepAngle, 360.0f);
}



void RadarWidgetBase::setPlayerTransform(
    const glm::vec3& pos,
    const glm::mat4& orientation)
{         
    m_playerPos = pos;
    m_playerOrientation = orientation;
}





void RadarWidgetBase::setContacts(
    const std::vector<RadarContactView>& contacts)
{
    m_contacts = contacts;
}





void RadarWidgetBase::render(
    const Viewport& vp,
    float parentX,
    float parentY,
    float parentW,
    float parentH
)
{
    float px, py, pw, ph;

    computeLayout(parentX, parentY, parentW, parentH,
                  px, py, pw, ph);

    m_centerX = px + pw * 0.5f;
    m_centerY = py + ph * 0.5f;
    m_radius  = std::max(pw, ph) * 0.45f;

    renderBackground(); 
    renderSweep();
    renderContacts();
    renderOverlay();

    renderChildren(vp, px, py, pw, ph);

    

}





RadarWidgetBase::~RadarWidgetBase()
{
    std::cout << "Radar destroyed\n";
}



void RadarWidgetBase::renderContacts()
{
    for (const auto& contact : m_contacts)
    {

        const glm::vec3& local = contact.Position;

        float displayRange = m_displayRange;

        float dist2D =
            std::sqrt(local.x * local.x + local.z * local.z);

        if (dist2D > displayRange)
            continue;

        float nx = local.x / displayRange;
        float nz = local.z / displayRange;

        float screenX = m_centerX + nx * m_radius;
        float screenY = m_centerY + nz * m_radius;

        glUseProgram(0);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        glPointSize(5.0f);
        glColor3f(0.0f, 1.0f, 0.0f);

        glBegin(GL_POINTS);
        glVertex2f(screenX, screenY);
        glEnd();
    }
}




void RadarWidgetBase::renderSweep()
{
    float rad = glm::radians(m_sweepAngle);

    float x = m_centerX + std::cos(rad) * m_radius;
    float y = m_centerY + std::sin(rad) * m_radius;

    glColor3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_LINES);
    glVertex2f(m_centerX, m_centerY);
    glVertex2f(x, y);
    glEnd();
}



void RadarWidgetBase::configureFromDesc(
        float sweepSpeed,
        float displayRange)
    {
        m_sweepSpeed            = sweepSpeed;
        m_displayRange          = displayRange;
    }



void RadarWidgetBase::applyVisualConfig(
    const IRadarVisualConfig& cfg)
    {
    }