#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{
namespace
{
const std::array<ChunkSpec, 14> ManifestChunksV4 {{
    {{{'M','E','T','A'}}, writeMeta, readMeta},
    {{{'M','A','T','L'}}, writeMaterials, readMaterials},
    {{{'S','E','M','N'}}, writeSemanticNodesV4, readSemanticNodesV4},
    {{{'S','T','A','T'}}, writeStateVariantsV4, readStateVariantsV4},
    {{{'C','O','L','L'}}, writeCollisionsV4, readCollisionsV4},
    {{{'S','O','C','K'}}, writeSocketsV4, readSocketsV4},
    {{{'S','M','E','T'}}, writeSocketMetadataV4, readSocketMetadataV4},
    {{{'H','I','T','R'}}, writeHitRegionsV4, readHitRegionsV4},
    {{{'O','P','E','N'}}, writeOpeningsV4, readOpeningsV4},
    {{{'R','E','P','R'}}, writeRepairTargetsV4, readRepairTargetsV4},
    {{{'S','I','Z','E'}}, writePhysicalSizeV4, readPhysicalSizeV4},
    {{{'S','T','R','L'}}, writeStructuralLinksV4, readStructuralLinksV4},
    {{{'L','O','D','S'}}, writeLodManifestV4, readLodManifestV4},
    {{{'L','E','R','R'}}, writeLodScreenErrorV4, readLodScreenErrorV4}
}};

const std::array<ChunkSpec, 6> ManifestChunksV3 {{
    {{{'M','E','T','A'}}, writeMeta, readMeta},
    {{{'M','A','T','L'}}, writeMaterials, readMaterials},
    {{{'G','E','O','M'}}, writeGeomManifest, readGeomManifest},
    {{{'N','O','D','E'}}, writeNodesLegacy, readNodesLegacy},
    {{{'C','O','L','L'}}, writeCollisionsLegacy, readCollisionsLegacy},
    {{{'S','O','C','K'}}, writeSocketsLegacy, readSocketsLegacy}
}};
}

const std::array<ChunkSpec, 14>& manifestChunksV4()
{
    return ManifestChunksV4;
}

const ChunkSpec* findManifestChunk(const std::array<char, 4>& id, bool v4)
{
    if (v4)
    {
        for (const auto& chunk : ManifestChunksV4) if (chunk.id == id) return &chunk;
    }
    else
    {
        for (const auto& chunk : ManifestChunksV3) if (chunk.id == id) return &chunk;
    }
    return nullptr;
}

ChunkReader findLegacyChunkReader(const std::array<char, 4>& id)
{
    if (id == std::array<char,4>{{'M','E','T','A'}}) return readMeta;
    if (id == std::array<char,4>{{'M','A','T','L'}}) return readMaterials;
    if (id == std::array<char,4>{{'G','E','O','M'}}) return readGeomLegacyV2;
    if (id == std::array<char,4>{{'N','O','D','E'}}) return readNodesLegacy;
    if (id == std::array<char,4>{{'C','O','L','L'}}) return readCollisionsLegacy;
    if (id == std::array<char,4>{{'S','O','C','K'}}) return readSocketsLegacy;
    return nullptr;
}

} // namespace elite::model_asset::binary
