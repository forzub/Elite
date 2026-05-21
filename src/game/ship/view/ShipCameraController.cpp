

#include <iostream>
#include "ShipCameraController.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/ship/view/ShipAttachmentResolver.h"

void ShipCameraController::updateMode(
    ShipCameraMode mode,
    float,
    const ShipDescriptor* desc,
    const ShipTransform& transform,
    Camera& camera,
    const std::vector<game::simulation::ObjectDetachedFragmentSnapshot>* detachedFragments
)
{
    switch (mode)
    {
        case ShipCameraMode::Cockpit:
        {
            auto resolved = resolveShipAttachment(
                desc,
                transform,
                "camera_cockpit_main",
                m_attachmentOverrides,
                detachedFragments
            );

            if (resolved)
            {

                camera.setPosition(resolved->localPosition);
                camera.setOrientationMatrix(resolved->worldOrientation);
            }
            else
            {
                // Камера теперь живёт в локальном player-frame.
                // Корабль игрока в рендере находится около (0,0,0).
                camera.setPosition(glm::vec3(0.0f));
                camera.setOrientationMatrix(transform.orientation);
            }
            break;
        }

        case ShipCameraMode::Rear:
        {
            auto resolved = resolveShipAttachment(
                desc,
                transform,
                "camera_rear_main",
                m_attachmentOverrides,
                detachedFragments
            );

            if (resolved)
            {

                camera.setPosition(resolved->localPosition);
                camera.setOrientationMatrix(resolved->worldOrientation);
            }
            else
            {
                glm::mat4 rearOrientation =
                    transform.orientation *
                    glm::rotate(
                        glm::mat4(1.0f),
                        glm::radians(180.0f),
                        glm::vec3(0,1,0)
                    );

                camera.setPosition(glm::vec3(0.0f));
                camera.setOrientationMatrix(rearOrientation);
            }
            break;
        }

        case ShipCameraMode::Drone:
        {
            auto resolved = resolveShipAttachment(
                desc,
                transform,
                "camera_drone_default",
                m_attachmentOverrides,
                detachedFragments
            );

            if (resolved)
            {
                

                glm::vec3 pos = resolved->localPosition;

                glm::mat4 look =
                    glm::lookAt(
                        pos,
                        glm::vec3(0.0f),
                        glm::vec3(0,1,0)
                    );

                glm::mat4 world = glm::inverse(look);
                glm::mat4 rotation = glm::mat4(glm::mat3(world));

                camera.setPosition(pos);
                camera.setOrientationMatrix(rotation);
            }
            else
            {
                glm::vec3 pos = glm::vec3(0.0f, 30.0f, 0.0f);

                camera.setPosition(pos);

                glm::mat4 look =
                    glm::lookAt(
                        pos,
                        glm::vec3(0.0f),
                        glm::vec3(0,1,0)
                    );

                glm::mat4 world = glm::inverse(look);
                glm::mat4 rotation = glm::mat4(glm::mat3(world));
                camera.setOrientationMatrix(rotation);
            }

            break;
        }

        default:
            break;
    }
}