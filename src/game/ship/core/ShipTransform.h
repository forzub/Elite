// ShipState что с ним сейчас#pragma once
// состояние корабля в настоящий момент
// Это единственный mutable-блок.
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct ShipTransform
{
    // позиция
    glm::vec3 position;
    glm::mat4 orientation {1.0f};

    float pitchRate;
    float yawRate;
    float rollRate;

    float pitchInput;
    float yawInput;
    float rollInput;

    // glm::vec3 localVelocity;
    // float forwardVelocity;


    // режимы
    bool cruiseActive = false;   // маршевый (удержание J)
    bool jumpActive   = false;   // прыжковый (отдельно)

    
    // ориентация
    // float pitch = 0.0f; // W/S
    // float yaw   = 0.0f; // Q/E
    // float roll  = 0.0f; // A/D

    // float pitchRate = 0.0f;
    // float yawRate   = 0.0f;
    // float rollRate  = 0.0f;

    // float pitchInput = 0.0f;
    // float yawInput   = 0.0f;
    // float rollInput  = 0.0f;

    // float speed = 0.0f; // throttle
    float targetSpeed = 0.0f;   // какую скорость хочет пилот
    float forwardVelocity = 0.0f; // реальная скорость
    float targetSpeedRate = 0.0f; // скорость изменения целевой скорости


    // локальная скорость (в системе корабля)
    glm::vec3 localVelocity {0.0f, 0.0f, 0.0f};

    // input манёвровых
    float strafeInput = 0.0f;   // влево / вправо
    float liftInput   = 0.0f;   // вверх / вниз
    float forwardInput = 0.0f; // манёвровое вперёд / назад

    



    glm::vec3 forward() const
    {
        return glm::normalize(glm::vec3(orientation * glm::vec4(0,0,-1,0)));
    }

    glm::vec3 right() const
    {
        return glm::normalize(glm::vec3(orientation * glm::vec4(1,0,0,0)));
    }

    glm::vec3 up() const
    {
        return glm::normalize(glm::vec3(orientation * glm::vec4(0,1,0,0)));
    }



    // glm::mat4 orientationMatrix() const
    // {
    //     glm::mat4 m(1.0f);

    //     m = glm::rotate(m, glm::radians(yaw),   glm::vec3(0,1,0));
    //     m = glm::rotate(m, glm::radians(pitch), glm::vec3(1,0,0));
    //     m = glm::rotate(m, glm::radians(roll),  glm::vec3(0,0,1));

    //     return m;
    // }
};
