#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:

Camera();

// Установка из внешней логики (корабль)
void setPosition(const glm::vec3& pos);
void setOrientation(float pitch, float yaw, float roll);

// Получение матрицы вида
glm::mat4 viewMatrix() const;
void setAspect(float aspect);
float aspect() const;

void setVisualLean(float rollOffset, float pitchOffset);
void setPerspective(float fovDeg, float nearPlane, float farPlane);
glm::mat4 projectionMatrix() const;

private:
    glm::vec3 m_position {0.0f, 0.0f, 0.0f};

    float m_pitch = 0.0f;
    float m_yaw   = 0.0f;
    float m_roll  = 0.0f;

    // наклон камеры при маневре
    float m_visualRoll  = 0.0f;
    float m_visualPitch = 0.0f;
    float m_aspect = 16.0f / 9.0f; // безопасный дефолт

    float m_fov   = 70.0f;
    float m_near  = 0.1f;
    float m_far   = 5000.0f;
};
