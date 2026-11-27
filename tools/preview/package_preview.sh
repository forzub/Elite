#!/usr/bin/env bash
# HantedPreview: the closed preview package, reproducibly (audit #8, the author 2026-11-27).
#
#   tools/preview/package_preview.sh [DEST]          (default DEST: D:/__elite/prodaction)
#
# 1. configures + builds build-release (Release, ELITE_PREVIEW_BUILD=ON, client + server) - the build's own steps copy
#    the runtime assets, build the UI pack and stage the MinGW runtime DLLs beside the executables;
# 2. stages Hanted.exe, HantedServer.exe, every non-system DLL they import (recursively, checked with objdump),
#    the runtime assets (minus developer leftovers), README.txt, BUILD_INFO.txt;
# 3. checks: every import resolved inside the package or by Windows; no developer paths in the text resources.
#
# DEST must be absent or empty: the script never deletes anything (remove an old package by hand first).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEST="${1:-D:/__elite/prodaction}"
BUILD="${ROOT}/build-release"
MINGW_BIN="/c/msys64/mingw64/bin"
export PATH="${MINGW_BIN}:${PATH}"

if [ -d "${DEST}" ] && [ -n "$(ls -A "${DEST}" 2>/dev/null)" ]; then
    echo "[preview] ${DEST} is not empty - remove the old package first (this script never deletes)" >&2
    exit 1
fi

echo "[preview] configure ${BUILD}"
cmake -S "${ROOT}" -B "${BUILD}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DELITE_BUILD_CLIENT=ON -DELITE_BUILD_SERVER=ON \
    -DELITE_PREVIEW_BUILD=ON \
    -DCMAKE_C_COMPILER="${MINGW_BIN}/cc.exe" -DCMAKE_CXX_COMPILER="${MINGW_BIN}/c++.exe" > /dev/null

echo "[preview] build (Release)"
ninja -C "${BUILD}" EliteGame EliteServer

mkdir -p "${DEST}"
echo "[preview] stage executables"
cp "${BUILD}/EliteGame.exe" "${DEST}/Hanted.exe"
cp "${BUILD}/EliteServer.exe" "${DEST}/HantedServer.exe"
strip --strip-debug "${DEST}/Hanted.exe" "${DEST}/HantedServer.exe"

# the non-system DLLs, recursively (the same rule as cmake/StageMinGWRuntime.cmake: found in the build dir or MinGW)
echo "[preview] stage runtime DLLs"
declare -A seen=()
queue=("${DEST}/Hanted.exe" "${DEST}/HantedServer.exe")
missing=()
while [ ${#queue[@]} -gt 0 ]; do
    f="${queue[0]}"; queue=("${queue[@]:1}")
    while read -r dll; do
        key="$(echo "${dll}" | tr 'A-Z' 'a-z')"
        [ -n "${seen[$key]:-}" ] && continue
        seen[$key]=1
        if [ -f "${BUILD}/${dll}" ]; then src="${BUILD}/${dll}"
        elif [ -f "${MINGW_BIN}/${dll}" ]; then src="${MINGW_BIN}/${dll}"
        else continue   # a Windows system DLL (checked below against System32)
        fi
        cp "${src}" "${DEST}/${dll}"
        queue+=("${DEST}/${dll}")
    done < <(objdump -p "${f}" | sed -n 's/^\s*DLL Name: //p' | tr -d '\r')
done

echo "[preview] stage assets"
mkdir -p "${DEST}/assets"
# the runtime asset tree the build made; developer leftovers left out
( cd "${BUILD}/assets" && tar cf - \
    --exclude='./models/layout_sketches' \
    --exclude='*.blend' --exclude='*.blend1' --exclude='*.psd' --exclude='*.kra' --exclude='*.xcf' \
    --exclude='./generated/celestial/*/*/lod/tiles' \
    --exclude='*.log' \
    . ) | ( cd "${DEST}/assets" && tar xf - )

echo "[preview] docs"
cp "${ROOT}/tools/preview/README.txt" "${DEST}/README.txt"
PROTOCOL="$(sed -n 's/.*ProtocolVersion\s*=\s*\([0-9]*\).*/\1/p' "${ROOT}/src/game/network/WireProtocol.h" | head -1)"
SCHEMA="$(sed -n 's/.*SchemaVersion\s*=\s*\([0-9]*\).*/\1/p' "${ROOT}/src/game/network/WireDataCodec.h" | head -1)"
COMMIT="$(git -C "${ROOT}" rev-parse HEAD)"
DIRTY="$(git -C "${ROOT}" status --porcelain -- src CMakeLists.txt tools | grep -v belt_demo_haze | head -1 || true)"
cat > "${DEST}/BUILD_INFO.txt" <<EOF
Hanted - closed preview build (experimental, NOT a release)
Build date (UTC): $(date -u +%Y-%m-%dT%H:%M:%SZ)
Git commit: ${COMMIT}$( [ -n "${DIRTY}" ] && echo " (+ uncommitted changes)" )
Network protocol version: ${PROTOCOL:-unknown}; wire data schema: ${SCHEMA:-unknown}
Configuration: CMake Release (-O3), MinGW-w64 GCC $(gcc -dumpversion), ELITE_PREVIEW_BUILD=ON, debug info stripped
Executables: Hanted.exe (game, single player runs its own local server), HantedServer.exe (optional dedicated server)
EOF

echo "[preview] checks"
# 1. every import resolved: in the package or a Windows system DLL
fail=0
for f in "${DEST}"/*.exe "${DEST}"/*.dll; do
    while read -r dll; do
        [ -f "${DEST}/${dll}" ] && continue
        [ -f "/c/Windows/System32/${dll}" ] && continue
        echo "  UNRESOLVED: $(basename "${f}") -> ${dll}"; fail=1
    done < <(objdump -p "${f}" | sed -n 's/^\s*DLL Name: //p' | tr -d '\r')
done
# 2. no developer paths in the text resources
if grep -rIl --include='*.json' --include='*.html' --include='*.js' --include='*.txt' -e '__elite' -e 'msys64' "${DEST}/assets" 2>/dev/null | head -5 | grep -q .; then
    echo "  WARNING: developer paths in:"; grep -rIl --include='*.json' --include='*.html' --include='*.js' --include='*.txt' -e '__elite' -e 'msys64' "${DEST}/assets" | head -5
fi
[ ${fail} -eq 0 ] && echo "  imports: all resolved"
echo "[preview] package: $(du -sh "${DEST}" | cut -f1), $(find "${DEST}" -type f | wc -l) files -> ${DEST}"
exit ${fail}
