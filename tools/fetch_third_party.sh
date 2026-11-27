#!/usr/bin/env bash
# Restores the vendored sources under third_party/ (ignored by git) at their pinned versions.
#   tools/fetch_third_party.sh            - fills only the missing / empty folders
# 2026-11-27: written after third_party/{asio,miniaudio,websocketpp,webview,webview2} were lost (a git worktree removal
# followed junctions into them). Versions: THIRD_PARTY_LICENSES.md, "Vendored sources".
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TP="${ROOT}/third_party"
TMP="$(mktemp -d)"
fetch() {   # <folder> <url> <top dir inside the archive>
    local dir="$1" url="$2" top="$3"
    if [ -d "${TP}/${dir}" ] && [ -n "$(ls -A "${TP}/${dir}" 2>/dev/null)" ]; then
        echo "[third_party] ${dir}: present"
        return
    fi
    echo "[third_party] ${dir}: fetching ${url}"
    curl -sSL -o "${TMP}/${dir}.tgz" "${url}"
    mkdir -p "${TMP}/${dir}" "${TP}/${dir}"
    tar xzf "${TMP}/${dir}.tgz" -C "${TMP}/${dir}"
    cp -r "${TMP}/${dir}/${top}/." "${TP}/${dir}/"
}
fetch asio "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-2.tar.gz" "asio-asio-1-28-2"
fetch webview "https://github.com/webview/webview/archive/refs/tags/0.12.0.tar.gz" "webview-0.12.0"
fetch miniaudio "https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz" "miniaudio-0.11.21"
fetch websocketpp "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz" "websocketpp-0.8.2"
# webview2: the Microsoft.Web.WebView2 1.0.1150.38 NuGet package (the build itself fetches it into _deps; this copy is
# not referenced by CMake)
if [ ! -d "${TP}/webview2" ] || [ -z "$(ls -A "${TP}/webview2" 2>/dev/null)" ]; then
    echo "[third_party] webview2: fetching the NuGet package"
    curl -sSL -o "${TMP}/webview2.zip" "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.1150.38"
    mkdir -p "${TP}/webview2"
    (cd "${TP}/webview2" && unzip -q "${TMP}/webview2.zip")
fi
echo "[third_party] done"
