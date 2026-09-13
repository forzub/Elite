#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace elite::model_asset::binary_v5_draft
{

// DRAFT ONLY.
// These types mirror the proposed v5 wire schema for review/tests. Production
// ModelAssetBinary continues to write/read v4 until a separate migration patch.
//
// IMPORTANT: never serialize these C++ structs with a raw fwrite/write. v5 is an
// explicitly little-endian wire format; every scalar must be encoded/decoded
// field-by-field.
inline constexpr std::uint16_t FormatMajor = 5;
inline constexpr std::uint16_t FormatMinor = 0;
inline constexpr std::uint16_t HeaderBytes = 64;
inline constexpr std::uint16_t DirectoryEntryBytes = 48;
inline constexpr std::uint32_t LittleEndianTag = 0x01020304u;
inline constexpr std::uint32_t NoLogicalIndex = 0xffffffffu;

inline constexpr std::array<char, 8> ManifestMagic{'E','L','M','D','L','0','0','5'};
inline constexpr std::array<char, 8> LodMagic{'E','L','M','S','H','0','0','5'};

enum HeaderFlags : std::uint32_t
{
    HeaderFlagNone = 0u,
    HeaderFlagDirectoryChecksum = 1u << 0
};

enum ChunkFlags : std::uint16_t
{
    ChunkFlagNone = 0u,
    ChunkFlagRequired = 1u << 0,
    ChunkFlagCompressed = 1u << 1,
    ChunkFlagStreamable = 1u << 2,
    ChunkFlagEditorOnly = 1u << 3
};

// Schema mirror only. Offsets below describe the wire layout, not a native ABI.
struct FileHeaderSchema
{
    std::array<char, 8> magic;          // 0
    std::uint16_t headerBytes;          // 8
    std::uint16_t versionMajor;         // 10
    std::uint16_t versionMinor;         // 12
    std::uint16_t directoryEntryBytes;  // 14
    std::uint32_t endianTag;            // 16
    std::uint32_t flags;                // 20
    std::uint32_t directoryCount;       // 24
    std::uint32_t reserved0;            // 28
    std::uint64_t directoryOffset;      // 32
    std::uint64_t fileBytes;            // 40
    std::array<std::uint8_t, 16> packageId; // 48
};

// Directory lives at the end of the file. This lets a writer stream payloads,
// emit the directory, then patch only the fixed 64-byte header.
struct ChunkDirectoryEntrySchema
{
    std::array<char, 4> id;          // FourCC
    std::uint16_t schemaVersion;
    std::uint16_t flags;
    std::uint64_t offset;
    std::uint64_t storedBytes;
    std::uint64_t decodedBytes;
    std::uint64_t checksum;
    std::uint32_t logicalIndex;
    std::uint32_t reserved0;
};

static_assert(sizeof(FileHeaderSchema) == HeaderBytes);
static_assert(sizeof(ChunkDirectoryEntrySchema) == DirectoryEntryBytes);

inline constexpr std::string_view ManifestStringTable = "STRS";
inline constexpr std::string_view ManifestMetadata = "META";
inline constexpr std::string_view ManifestMaterials = "MATL";
inline constexpr std::string_view ManifestSemantics = "SEMN";
inline constexpr std::string_view ManifestStates = "STAT";
inline constexpr std::string_view ManifestRigidBody = "RBOD";
inline constexpr std::string_view ManifestCollision = "COLL";
inline constexpr std::string_view ManifestSockets = "SOCK";
inline constexpr std::string_view ManifestSocketMetadata = "SMET";
inline constexpr std::string_view ManifestHitRegions = "HITR";
inline constexpr std::string_view ManifestOpenings = "OPEN";
inline constexpr std::string_view ManifestRepairTargets = "REPR";
inline constexpr std::string_view ManifestStructuralLinks = "STRL";
inline constexpr std::string_view ManifestLodIndex = "LODS";
inline constexpr std::string_view ManifestLodErrors = "LERR";

inline constexpr std::string_view LodInfo = "LINF";
inline constexpr std::string_view LodRenderGraph = "RGRF";
inline constexpr std::string_view LodMesh = "MESH";

} // namespace elite::model_asset::binary_v5_draft
