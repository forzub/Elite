#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tools/model_asset_editor"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DELITE_BUILD_CLIENT=OFF \
    -DELITE_BUILD_SERVER=OFF \
    -DELITE_BUILD_ASSET_EDITOR=ON \
    -DELITE_MODEL_ASSET_LIBIGL_SPIKE=ON
cmake --build "${BUILD_DIR}" --target EliteAssetEditor model_asset_libigl_spike
BIN_DIR="${ROOT_DIR}/build/tools/model_asset_editor/bin"
printf '\nBuilt: %s\n' "${BIN_DIR}/EliteAssetEditor.exe"
printf 'Spike: %s\n' "${BIN_DIR}/model_asset_libigl_spike.exe"
printf 'Run:   %s\n' "${BIN_DIR}/EliteAssetEditor.exe"
