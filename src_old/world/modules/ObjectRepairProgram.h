#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::modules
{

enum class ObjectRepairStepKind
{
    FlyToPoint,
    CaptureFragment,
    TowFragmentToPoint,
    AlignFragmentToMount,
    InsertFragmentToMount,
    AttachFragment,
    ReturnToDock
};

struct ObjectRepairStep
{
    ObjectRepairStepKind kind = ObjectRepairStepKind::FlyToPoint;

    std::string label;

    glm::vec3 targetPoint {0.0f};
    glm::mat4 targetOrientation {1.0f};

    bool mustStop = true;
    bool useObstacleAvoidance = true;
    bool towFragment = false;

    float speed = 20.0f;
    float acceleration = 80.0f;
    float arrivalRadius = 0.5f;
    float duration = 0.0f;
};

struct ObjectRepairProgram
{
    std::string name;
    std::vector<ObjectRepairStep> steps;
    int currentStep = 0;
};

} // namespace world::modules