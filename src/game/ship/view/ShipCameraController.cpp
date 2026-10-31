

#include <iostream>
#include <algorithm>

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
                camera.setPosition(glm::vec3(0.0f));

                camera.setOrientationMatrix(
                    transform.orientation *
                    glm::rotate(
                        glm::mat4(1.0f),
                        glm::radians(180.0f),
                        glm::vec3(0, 1, 0)
                    )
                );
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

            // Recon drone: an EXTERNAL camera in the ship frame, behind and
            // above, looking at the ship. The descriptor attachment gives the
            // direction; the drone never sits closer than kMinDroneRangeM
            // (the old 9 m attachment put the camera inside the hull).
            constexpr float kMinDroneRangeM = 90.0f;
            const glm::mat3 shipBasis = glm::mat3(transform.orientation);
            glm::vec3 local =
                resolved
                    ? glm::transpose(shipBasis) * resolved->localPosition
                    : glm::vec3(0.0f);
            const glm::vec3 chase(0.0f, 0.28f, 1.0f); // +Z = aft, +Y = up
            if (glm::length(local) < kMinDroneRangeM)
                local = glm::normalize(chase) * kMinDroneRangeM;

            const glm::vec3 pos = shipBasis * local;
            const glm::vec3 shipUp = shipBasis * glm::vec3(0, 1, 0);

            camera.setPosition(pos);

            glm::mat4 look =
                glm::lookAt(
                    pos,
                    glm::vec3(0.0f),
                    shipUp
                );

            glm::mat4 world = glm::inverse(look);
            glm::mat4 rotation = glm::mat4(glm::mat3(world));

            camera.setOrientationMatrix(rotation);

            break;
        }

        default:
            break;
    }
}