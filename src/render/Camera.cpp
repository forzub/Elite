#include "Camera.h"

Camera::Camera() = default;

void Camera::setPosition(const glm::vec3& pos)
{
    m_position = pos;
}


glm::mat4 Camera::viewMatrix() const
{
    glm::mat4 world =
        glm::translate(glm::mat4(1.0f), m_position) *
        m_orientation;

    return glm::inverse(world);
}


void Camera::setOrientationMatrix(const glm::mat4& orientation)
{
    m_orientation = orientation;
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

void Camera::setPerspective(float fovDeg, float nearPlane, float farPlane)
{
    m_fov  = fovDeg;
    m_near = nearPlane;
    m_far  = farPlane;
}

glm::mat4 Camera::projectionMatrix() const
{
    return glm::perspective(
        glm::radians(m_fov),
        m_aspect,
        m_near,
        m_far
    );
}

