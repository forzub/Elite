#pragma once

#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/binary/ModelAssetBinaryWire.h"

namespace elite::model_asset::binary
{

void writeMeshLod(Writer& w, const MeshLod& lod);
void readMeshLod(Reader& r, MeshLod& lod);

} // namespace elite::model_asset::binary
