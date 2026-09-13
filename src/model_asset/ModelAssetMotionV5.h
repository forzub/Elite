#pragma once

// PROVISIONAL v5 motion/schema design types.
//
// These types deliberately do NOT participate in the active ModelAsset v4 wire
// serializer yet. They are the concrete compile-time design target for the v5
// manifest/.elmesh/.elanim implementation and may be revised during production
// acceptance of articulated + skinned assets.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace elite::model_asset::v5_draft
{

constexpr std::uint32_t MotionSchemaVersion = 1;
constexpr std::int32_t DraftNoIndex = -1;

enum class MotionJointType : std::uint8_t
{
    Fixed = 0,
    Revolute = 1,
    Prismatic = 2
};

enum class MotionChannelType : std::uint8_t
{
    ScalarAngleDeg = 0,
    ScalarDistance = 1
};

enum class AttachmentTargetKind : std::uint8_t
{
    SemanticNode = 0,
    RigNode = 1,
    Bone = 2
};

enum class AnimationTrackTargetKind : std::uint8_t
{
    MotionChannel = 0,
    Bone = 1
};

enum AnimationClipFlag : std::uint32_t
{
    ClipLoop = 1u << 0,
    ClipHasRootMotion = 1u << 1
};

struct TransformTRS
{
    glm::vec3 translation {0.0f};
    glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale {1.0f};
};

// Rigid articulated transform node. This is not a SemanticNode and not a Bone.
struct RigNode
{
    std::string id;
    std::int32_t parentIndex = DraftNoIndex;
    TransformTRS restLocal;
};

struct RigJoint
{
    std::string id;
    std::int32_t parentRigNodeIndex = DraftNoIndex;
    std::int32_t childRigNodeIndex = DraftNoIndex;
    MotionJointType type = MotionJointType::Fixed;
    glm::vec3 axis {0.0f, 1.0f, 0.0f};
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float neutralValue = 0.0f;
    bool detachable = false;
};

struct RigDefinition
{
    std::string id;
    std::vector<RigNode> nodes;
    std::vector<RigJoint> joints;
};

// Stable logical channel presented to runtime drivers. Clips and direct/Aim
// drivers address the same channel instead of mutating arbitrary transforms.
struct MotionChannel
{
    std::string id;
    MotionChannelType type = MotionChannelType::ScalarAngleDeg;
    std::uint32_t rigIndex = 0;
    std::uint32_t jointIndex = 0;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float defaultValue = 0.0f;
};

struct SkeletonBone
{
    std::string id;
    std::int32_t parentIndex = DraftNoIndex;
    TransformTRS restLocal;
};

struct SkeletonDefinition
{
    std::string id;
    std::vector<SkeletonBone> bones;
};

// Compiled per-vertex skin payload. Four influences are the initial compiled
// limit; the v5 semantic contract does not assume four forever.
struct SkinInfluence4
{
    std::array<std::uint16_t, 4> paletteIndices {{0, 0, 0, 0}};
    glm::vec4 weights {1.0f, 0.0f, 0.0f, 0.0f};
};

// Lives with one .elmesh LOD payload and references an asset-wide skeleton.
struct LodSkinBinding
{
    std::string id;
    std::uint32_t skeletonIndex = 0;
    std::vector<std::uint32_t> bonePalette;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<SkinInfluence4> vertexInfluences;
};

struct AnimationEvent
{
    float timeSeconds = 0.0f;
    std::string id;
    std::string payload;
};

// Lightweight manifest entry. Heavy keys are stored in <asset>.elanim.
struct AnimationClipDescriptor
{
    std::string id;
    float durationSeconds = 0.0f;
    std::uint32_t flags = 0;
    std::int32_t rootMotionBoneIndex = DraftNoIndex;
    std::uint32_t firstTrack = 0;
    std::uint32_t trackCount = 0;
    std::uint32_t firstEvent = 0;
    std::uint32_t eventCount = 0;
};

struct ScalarKey
{
    float timeSeconds = 0.0f;
    float value = 0.0f;
};

struct BoneTrsKey
{
    float timeSeconds = 0.0f;
    TransformTRS value;
};

// Directory record inside .elanim. A rigid track targets MotionChannel; a
// skeletal track targets a bone in the clip's referenced shared skeleton.
struct AnimationTrackDescriptor
{
    AnimationTrackTargetKind targetKind = AnimationTrackTargetKind::MotionChannel;
    std::uint32_t targetIndex = 0;
    std::uint32_t firstKey = 0;
    std::uint32_t keyCount = 0;
};

struct AttachmentBinding
{
    std::string id;
    AttachmentTargetKind targetKind = AttachmentTargetKind::SemanticNode;
    std::int32_t semanticNodeIndex = DraftNoIndex;
    std::int32_t rigIndex = DraftNoIndex;
    std::int32_t targetIndex = DraftNoIndex; // RigNode or Bone according to targetKind.
    TransformTRS local;
};

// Proposed shared v5 manifest motion catalog. Runtime driver state is purposely
// absent: this describes capabilities/data, not AI/gameplay decisions.
struct MotionCatalog
{
    std::vector<RigDefinition> rigs;
    std::vector<MotionChannel> channels;
    std::vector<SkeletonDefinition> skeletons;
    std::vector<AnimationClipDescriptor> clips;
    std::vector<AnimationEvent> events;
    std::vector<AttachmentBinding> attachments;
};

} // namespace elite::model_asset::v5_draft
