#pragma once

#include <string>

class IObjectDescriptor
{
public:
    virtual ~IObjectDescriptor() = default;

    virtual const std::string& meshId() const = 0;
    virtual bool isLargeObject() const = 0;
    virtual glm::vec3 getMeshSizeMeters() const = 0;
};