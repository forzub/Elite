// #pragma once

// #include <string>
// #include <vector>
// #include <glm/glm.hpp>

// #include "src/world/descriptors/ModuleDescriptor.h"

// class IObjectDescriptor
// {
// public:
//     virtual ~IObjectDescriptor() = default;

//     virtual const std::string& meshId() const = 0;
//     virtual bool isLargeObject() const = 0;
//     virtual glm::vec3 getMeshSizeMeters() const = 0;

//     virtual const glm::vec3& visualBasisRotationDeg() const = 0;

//     virtual const std::vector<ModuleDescriptor>& moduleDescriptors() const = 0;
// };



#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <cmath>

#include "src/world/descriptors/ModuleDescriptor.h"
#include "src/world/descriptors/LogicalDimensions.h"

class IObjectDescriptor
{
public:
    virtual ~IObjectDescriptor() = default;

    virtual const std::string& meshId() const = 0;
    virtual bool isLargeObject() const = 0;
    virtual glm::vec3 getMeshSizeMeters() const = 0;
    virtual const LogicalDimensions& logicalDimensions() const = 0;

    // LEGACY: старый визуальный костыль.
    // Пока оставляем только для старых single-mesh путей.
    virtual const glm::vec3& visualBasisRotationDeg() const = 0;

    // НОВОЕ: авторский базис меша
    virtual const glm::vec3& meshForwardAxis() const = 0;
    virtual const glm::vec3& meshUpAxis() const = 0;

    virtual const std::vector<ModuleDescriptor>& moduleDescriptors() const = 0;

    // Строит матрицу, которая переводит координаты меша
    // из авторского локального базиса в логический игровой базис:
    // +X right, +Y up, -Z forward
    glm::mat3 meshToLogicalBasis() const
    {
        const glm::vec3 rawForward = meshForwardAxis();
        const glm::vec3 rawUp = meshUpAxis();

        const float eps = 1e-6f;

        if (glm::length(rawForward) < eps || glm::length(rawUp) < eps)
            return glm::mat3(1.0f);

        glm::vec3 srcForward = glm::normalize(rawForward);
        glm::vec3 srcUpGuess = glm::normalize(rawUp);

        if (std::abs(glm::dot(srcForward, srcUpGuess)) > 0.999f)
            return glm::mat3(1.0f);

        // Правая система:
        // right = forward x up
        glm::vec3 srcRight = glm::normalize(glm::cross(srcForward, srcUpGuess));
        glm::vec3 srcUp    = glm::normalize(glm::cross(srcRight, srcForward));

        // Целевой логический базис
        const glm::vec3 dstRight   = glm::vec3( 1.0f, 0.0f,  0.0f);
        const glm::vec3 dstUp      = glm::vec3( 0.0f, 1.0f,  0.0f);
        const glm::vec3 dstForward = glm::vec3( 0.0f, 0.0f, -1.0f);

        // Колонки матриц = базисные векторы
        const glm::mat3 srcBasis(srcRight, srcUp, srcForward);
        const glm::mat3 dstBasis(dstRight, dstUp, dstForward);

        // Перевод из basis source -> basis destination
        return dstBasis * glm::transpose(srcBasis);
    }

    glm::mat4 meshToLogicalBasis4() const
    {
        const glm::mat3 m = meshToLogicalBasis();

        glm::mat4 out(1.0f);
        out[0] = glm::vec4(m[0], 0.0f);
        out[1] = glm::vec4(m[1], 0.0f);
        out[2] = glm::vec4(m[2], 0.0f);
        out[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return out;
    }
};