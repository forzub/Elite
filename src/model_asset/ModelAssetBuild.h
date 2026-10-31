#pragma once

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset
{

// The single authoring-units -> metres boundary (Model Asset Editor BUILD and
// the headless EliteModelAssetBuild tool). Scales every authored distance;
// SI quantities (kg, N, Nm, light range) are not touched.
void scaleAuthoringDistancesUniform(ModelAsset& asset, float scale);

// BUILD: a metric production copy of a calibrated WORKING asset. Throws when
// the asset has no explicit authoring -> metre calibration.
ModelAsset makeProductionCopy(const ModelAsset& working);

} // namespace elite::model_asset
