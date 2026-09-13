#pragma once

#include <array>

#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/binary/ModelAssetBinaryWire.h"

namespace elite::model_asset::binary
{

using ChunkWriter = void (*)(Writer&, const ModelAsset&);
using ChunkReader = void (*)(Reader&, ModelAsset&);

struct ChunkSpec
{
    std::array<char, 4> id;
    ChunkWriter writer;
    ChunkReader reader;
};

// Shared META / MATL and physical-scale codecs.
void writeMeta(Writer&, const ModelAsset&);
void readMeta(Reader&, ModelAsset&);
void writeMaterials(Writer&, const ModelAsset&);
void readMaterials(Reader&, ModelAsset&);
void writePhysicalSizeV4(Writer&, const ModelAsset&);
void readPhysicalSizeV4(Reader&, ModelAsset&);

// Current semantic/runtime domains.
void writeSemanticNodesV4(Writer&, const ModelAsset&);
void readSemanticNodesV4(Reader&, ModelAsset&);
void writeStateVariantsV4(Writer&, const ModelAsset&);
void readStateVariantsV4(Reader&, ModelAsset&);
void writeCollisionsV4(Writer&, const ModelAsset&);
void readCollisionsV4(Reader&, ModelAsset&);
void writeSocketsV4(Writer&, const ModelAsset&);
void readSocketsV4(Reader&, ModelAsset&);
void writeSocketMetadataV4(Writer&, const ModelAsset&);
void readSocketMetadataV4(Reader&, ModelAsset&);
void writeHitRegionsV4(Writer&, const ModelAsset&);
void readHitRegionsV4(Reader&, ModelAsset&);
void writeOpeningsV4(Writer&, const ModelAsset&);
void readOpeningsV4(Reader&, ModelAsset&);
void writeRepairTargetsV4(Writer&, const ModelAsset&);
void readRepairTargetsV4(Reader&, ModelAsset&);
void writeStructuralLinksV4(Writer&, const ModelAsset&);
void readStructuralLinksV4(Reader&, ModelAsset&);
void writeLodManifestV4(Writer&, const ModelAsset&);
void readLodManifestV4(Reader&, ModelAsset&);
void writeLodScreenErrorV4(Writer&, const ModelAsset&);
void readLodScreenErrorV4(Reader&, ModelAsset&);

// Legacy v2/v3 compatibility is isolated from current codecs.
void writeGeomManifest(Writer&, const ModelAsset&);
void readGeomManifest(Reader&, ModelAsset&);
void readGeomLegacyV2(Reader&, ModelAsset&);
void writeNodesLegacy(Writer&, const ModelAsset&);
void readNodesLegacy(Reader&, ModelAsset&);
void writeCollisionsLegacy(Writer&, const ModelAsset&);
void readCollisionsLegacy(Reader&, ModelAsset&);
void writeSocketsLegacy(Writer&, const ModelAsset&);
void readSocketsLegacy(Reader&, ModelAsset&);

const std::array<ChunkSpec, 14>& manifestChunksV4();
const ChunkSpec* findManifestChunk(const std::array<char, 4>& id, bool v4);
ChunkReader findLegacyChunkReader(const std::array<char, 4>& id);

} // namespace elite::model_asset::binary
