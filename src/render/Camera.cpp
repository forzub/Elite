#include "Camera.h"

Camera::Camera() = default;

void Camera::setPosition(const glm::vec3& pos)
{
    m_position = pos;
}

void Camera::setOrientation(float pitch, float yaw, float roll)
{
    m_pitch = pitch;
    m_yaw   = yaw;
    m_roll  = roll;
}

glm::mat4 Camera::viewMatrix() const
{
    glm::mat4 view(1.0f);

    // Обратные вращения (камера)
    view = glm::rotate(view, glm::radians(-(m_roll + m_visualRoll)),  {0,0,1});
    view = glm::rotate(view, glm::radians(-(m_pitch + m_visualPitch)), {1,0,0});
    view = glm::rotate(view, glm::radians(-m_yaw),   {0,1,0});

    // Обратное смещение
    view = glm::translate(view, -m_position);

    return view;
}

void Camera::setVisualLean(float rollOffset, float pitchOffset)
{
    m_visualRoll  = rollOffset;
    m_visualPitch = pitchOffset;
}

void Camera::setAspect(float aspect)
{
    if (aspect > 0.0f)
        m_aspect = aspect;
}

float Camera::aspect() const
{
    return m_aspect;
}
