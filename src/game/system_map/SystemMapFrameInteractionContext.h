#pragma once

#include "src/game/system_map/SystemMapInteraction.h"

namespace game::system_map
{
    struct SystemMapControlSettings;
    struct SystemMapFrameData;

    /* Concrete semantic picking adapter over one prepared CPU frame. */
    class SystemMapFrameInteractionContext final
        : public SystemMapInteractionContext
    {
    public:
        SystemMapFrameInteractionContext(
            const SystemMapFrameData& frame,
            const SystemMapControlSettings& controls
        );

        std::optional<std::string> pickSystemBodyId(
            double localMouseX,
            double localMouseY
        ) const override;

        std::optional<SystemMapHubSelection> pickSystemHubSelection(
            double localMouseX,
            double localMouseY
        ) const override;

        std::optional<SystemMapCameraBodyTarget>
        pickSystemCameraBodyTarget(
            double localMouseX,
            double localMouseY,
            const Viewport& viewport
        ) const override;

        std::optional<glm::dvec3> systemBodyAbsolutePosition(
            const std::string& bodyId
        ) const override;

        std::optional<glm::dvec3> systemObjectAbsolutePosition(
            const std::string& objectId
        ) const override;

        double largestDirectBodyPhysicalSizeMetersAt(
            double localMouseX,
            double localMouseY
        ) const;

    private:
        int pickBody(double x, double y) const;
        int pickHub(double x, double y) const;
        int pickCameraBody(double x, double y) const;

        const SystemMapFrameData& m_frame;
        const SystemMapControlSettings& m_controls;
    };
}
