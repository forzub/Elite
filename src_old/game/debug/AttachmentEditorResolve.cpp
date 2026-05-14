#include "game/debug/AttachmentEditorResolve.h"
#include "game/SpaceState.h"
#include "game/ship/ShipDescriptor.h"
#include "game/ship/ShipAttachmentPoint.h"

std::optional<ShipAttachmentPoint> getEditedAttachmentPoint(
    const SpaceState& state,
    const ShipAttachmentPoint& basePoint)
{
    auto it = state.attachmentEditorOverrides().find(basePoint.id);
    if (it == state.attachmentEditorOverrides().end())
        return basePoint;

    ShipAttachmentPoint out = basePoint;
    out.localPosition = it->second.localPosition;
    out.localRotationDeg = it->second.localRotationDeg;
    out.enabled = it->second.enabled;
    return out;
}