#pragma once

#include <optional>
#include <glm/vec3.hpp>
#include "src/world/WorldSignal.h"
#include "src/scene/EntityID.h"

class ITransmitterSource
{
public:
    virtual ~ITransmitterSource() = default;

    virtual std::optional<WorldSignal> emitSignal() const = 0;
};