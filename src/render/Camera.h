#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    Camera();

    void setPosition(const glm::vec3& pos);
    void setOrientationMatrix(const glm::mat4& orientation);

    glm::mat4 viewMatrix() const;

    void setAspect(float aspect);
    float aspect() const;

    void setPerspective(float fovDeg, float nearPlane, float farPlane);
    glm::mat4 projectionMatrix() const;

    float fov() const { return m_fov; }
    float nearPlane() const { return m_near; }
    float farPlane() const { return m_far; }

    glm::vec3 position() const { return m_position; }

private:
    glm::vec3 m_position {0.0f, 0.0f, 0.0f};
    float m_aspect = 16.0f / 9.0f;
    float m_fov = 70.0f;
    float m_near = 0.1f;
    float m_far = 150000.0f;
    glm::mat4 m_orientation {1.0f};
};