#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "src/game/shared/EntityId.h"

struct RadarContactView;

class IRadarWidget
{
public:
    virtual ~IRadarWidget() = default;

    virtual void setPlacement(
        float centerX,
        float centerY,
        float radius) = 0;

    virtual void setPlayerTransform(
        const glm::vec3& pos,
        const glm::mat4& orientation) = 0;

    virtual void setContacts(
        const std::vector<RadarContactView>& contacts) = 0;

    virtual void update(float dt) = 0;
    virtual void render() = 0;
};