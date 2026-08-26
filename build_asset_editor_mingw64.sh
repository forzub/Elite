#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tools/model_asset_editor"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DELITE_BUILD_CLIENT=OFF \
    -DELITE_BUILD_SERVER=OFF \
    -DELITE_BUILD_ASSET_EDITOR=ON
cmake --build "${BUILD_DIR}" --target EliteAssetEditor
printf '\nBuilt: %s\n' "${BUILD_DIR}/EliteAssetEditor.exe"
printf 'Run:   %s\n' "${BUILD_DIR}/EliteAssetEditor.exe"
