#pragma once

#include "render/Camera.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/view/ShipCameraMode.h"
#include "src/game/ship/ShipDescriptor.h"
#include "game/debug/AttachmentEditorData.h"
#include <vector>
#include "src/game/simulation/ObjectDetachedFragmentSnapshot.h"



class ShipCameraController
{
public:
    void setAttachmentOverrides(const ShipAttachmentOverrideMap* overrides)
    {
        m_attachmentOverrides = overrides;
    }

    void updateMode(
        ShipCameraMode mode,
        float dt,
        const ShipDescriptor* desc,
        const ShipTransform& transform,
        Camera& camera,
        const std::vector<game::simulation::ObjectDetachedFragmentSnapshot>* detachedFragments = nullptr
    );

private:
    const ShipAttachmentOverrideMap* m_attachmentOverrides = nullptr;

};