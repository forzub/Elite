#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors = []


def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")

attachment_h = SRC / "game/simulation/HubAttachmentSnapshot.h"
object_snapshot_h = SRC / "game/simulation/ObjectSnapshot.h"
simulation_cpp = SRC / "game/simulation/GameSimulation.cpp"
client_world_h = SRC / "game/client/ClientWorldState.h"
client_world_cpp = SRC / "game/client/ClientWorldState.cpp"
basis_h = SRC / "game/navigation/HubFrameBasis.h"
presentation_h = SRC / "game/client/HubFramePresentation.h"
reference_presentation_h = SRC / "game/client/ReferenceFramePresentation.h"
scene_renderer_cpp = SRC / "scene/SceneRenderer.cpp"

for path in (
    attachment_h,
    object_snapshot_h,
    simulation_cpp,
    client_world_h,
    client_world_cpp,
    basis_h,
    presentation_h,
    scene_renderer_cpp,
):
    if not path.is_file():
        fail(path, "required hub co-frame presentation file is missing")

if attachment_h.is_file():
    text = attachment_h.read_text(encoding="utf-8", errors="replace")
    for required in (
        "struct HubAttachmentSnapshot",
        "int systemId = -1",
        "std::string hubId",
        "localOffsetMeters",
        "localRotationDeg",
        "inheritHubOrientation",
        "bool valid = false",
    ):
        if required not in text:
            fail(attachment_h, f"hub attachment contract is incomplete: {required}")

if object_snapshot_h.is_file():
    text = object_snapshot_h.read_text(encoding="utf-8", errors="replace")
    if "HubAttachmentSnapshot hubAttachment" not in text:
        fail(object_snapshot_h, "ObjectSnapshot does not carry stable hub binding")

if simulation_cpp.is_file():
    text = simulation_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "o.hubAttachment.systemId = obj.systemId",
        "o.hubAttachment.hubId = obj.hubId",
        "o.hubAttachment.localOffsetMeters = obj.hubLocalOffsetMeters",
        "o.hubAttachment.localRotationDeg = obj.hubLocalRotationDeg",
        "o.hubAttachment.valid = true",
        "game::navigation::hubVisualLocalToWorldVector(",
        "game::navigation::hubAttachedVisualOrientation(",
        "game::navigation::hubVisualOrientation(",
    ):
        if required not in text:
            fail(simulation_cpp, f"server snapshot loses hub binding: {required}")

if client_world_h.is_file():
    text = client_world_h.read_text(encoding="utf-8", errors="replace")
    if "HubAttachmentSnapshot hubAttachment" not in text:
        fail(client_world_h, "ClientObjectState does not retain hub binding")

if client_world_cpp.is_file():
    text = client_world_cpp.read_text(encoding="utf-8", errors="replace")
    if text.count("state.hubAttachment = o.hubAttachment") < 2:
        fail(client_world_cpp, "hub binding is not copied on create and update")


if basis_h.is_file():
    text = basis_h.read_text(encoding="utf-8", errors="replace")
    for required in (
        "hubVisualLocalToWorldVector(",
        "normalAxis * localVector.x",
        "radialAxis * localVector.y",
        "progradeAxis * localVector.z",
        "hubVisualOrientation(",
        "hubAttachedVisualOrientation(",
    ):
        if required not in text:
            fail(basis_h, f"shared hub visual basis is incomplete: {required}")

if presentation_h.is_file():
    text = presentation_h.read_text(encoding="utf-8", errors="replace")
    for required in (
        "canResolveHubLocalPosition(",
        "resolveHubLocalPosition(",
        "canResolveHubAttachedPresentation(",
        "attachment.systemId == frame.systemId",
        "attachment.hubId == frame.hubId",
        "hubVisualLocalToWorldPosition(",
        "hubAttachedVisualOrientation(",
        "resolveHubAttachedObjectPresentation(",
    ):
        if required not in text:
            fail(presentation_h, f"shared hub presentation frame is incomplete: {required}")

if reference_presentation_h.is_file():
    text = reference_presentation_h.read_text(encoding="utf-8", errors="replace")
    for required in (
        "sameReferenceFrameIdentity(",
        "interpolateReferenceFramePresentation(",
        "out.originMeters",
        "out.radialAxis",
        "out.progradeAxis",
        "out.normalAxis",
        "out.universeTimeSeconds",
    ):
        if required not in text:
            fail(reference_presentation_h, f"render-time frame sampler is incomplete: {required}")

if scene_renderer_cpp.is_file():
    text = scene_renderer_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "obj.hubAttachment",
        "itPlayer->second.renderReferenceFrame",
        "resolveHubAttachedObjectPresentation(",
        "canResolveHubLocalPosition(",
        "ship.renderTransform.motion.localPositionMeters",
        "hubPose.worldPositionMeters",
        "hubPose.worldOrientation",
    ):
        if required not in text:
            fail(scene_renderer_cpp, f"gameplay renderer still mixes hub epochs: {required}")

if client_world_cpp.is_file():
    text = client_world_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "sameReferenceFrame(",
        "sampleRenderReferenceFrame",
        "interpolateReferenceFramePresentation(",
        "itOld->referenceFrame.localPositionMeters",
        "itNew->referenceFrame.localPositionMeters",
        "ship.renderReferenceFrame.localPositionMeters",
        "ship.renderReferenceFrame.localToWorldPosition(",
    ):
        if required not in text:
            fail(client_world_cpp, f"co-frame ship interpolation is incomplete: {required}")


if client_world_cpp.is_file():
    text = client_world_cpp.read_text(encoding="utf-8", errors="replace")
    start = text.find("const bool usePredictedPlayerPresentation")
    end = text.find("// ===== 3", start)
    if start >= 0 and end > start:
        predicted_block = text[start:end]
        if "ship.renderReferenceFrame = ship.referenceFrame;" in predicted_block:
            fail(client_world_cpp, "predicted player still snaps the render frame to newest authoritative sample")

if errors:
    print("Hub co-frame presentation architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("Hub co-frame presentation architecture check passed.")
