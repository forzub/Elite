#pragma once
#include <glm/glm.hpp>


struct ShipTransform
{


    bool cruiseActive = false;   // маршевый (удержание J)
    bool jumpActive   = false;   // прыжковый (отдельно)
    
    glm::vec3 position {0, 5, 10};
    

    float pitch = 0.0f; // W/S
    float yaw   = 0.0f; // Q/E
    float roll  = 0.0f; // A/D

    float pitchRate = 0.0f;
    float yawRate   = 0.0f;
    float rollRate  = 0.0f;

    float pitchInput = 0.0f;
    float yawInput   = 0.0f;
    float rollInput  = 0.0f;

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
};
