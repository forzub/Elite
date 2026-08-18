#!/usr/bin/env python3
"""Architecture guard for the shared WebUI font/i18n/resource-pack platform."""

from __future__ import annotations

import json
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] UI platform/font contract: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


license_index = ROOT / "THIRD_PARTY_LICENSES.md"
ofl_path = ROOT / "src/assets/licenses/fonts/SIL-OFL-1.1.txt"
manifest_path = ROOT / "src/assets/ui/font_manifest.json"
css_path = ROOT / "src/assets/webui/elite_ui.css"
languages_path = ROOT / "src/assets/localization/languages.json"
fetcher_path = ROOT / "tools/fetch_ui_fonts.ps1"
fetch_wrapper_path = ROOT / "tools/fetch_ui_fonts_mingw64.sh"
pack_builder = ROOT / "tools/build_ui_pack.py"

for path in (license_index, ofl_path, manifest_path, css_path, languages_path, fetcher_path, fetch_wrapper_path, pack_builder):
    require(path.is_file(), f"required file is missing: {path.relative_to(ROOT)}")

license_text = license_index.read_text(encoding="utf-8")
ofl_text = ofl_path.read_text(encoding="utf-8")
require("Noto" in license_text and "SIL Open Font License 1.1" in license_text,
        "central third-party index does not register the bundled Noto fonts")
require("constellation_lines.NOTICE.txt" in license_text and "sky_cultures/NOTICE.md" in license_text,
        "central third-party index lost existing dataset provenance links")

notice_files = sorted(
    path for path in (ROOT / "src/assets").rglob("*")
    if path.is_file() and ("NOTICE" in path.name.upper() or "LICENSE" in path.name.upper())
    and path != ofl_path
)
for notice in notice_files:
    relative = notice.relative_to(ROOT).as_posix()
    require(relative in license_text,
            f"third-party asset notice is not indexed centrally: {relative}")
require("SIL OPEN FONT LICENSE Version 1.1" in ofl_text,
        "bundled OFL license text is not the expected OFL-1.1 notice")

manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
fonts = manifest.get("fonts", [])
require(manifest.get("schema_version") == 1, "font manifest schema is not version 1")
require(manifest.get("license") == "OFL-1.1", "font manifest does not declare OFL-1.1")
require(len(fonts) >= 25, "global font manifest is too small to cover the promised script set")

commit_re = re.compile(r"/[0-9a-f]{40}/")
for entry in fonts:
    require(entry.get("license") == "OFL-1.1", f"font {entry.get('id')} has an unexpected license")
    require(commit_re.search(entry.get("source", "")) is not None,
            f"font {entry.get('id')} is not pinned to an immutable 40-hex commit URL")

required_files = {
    "NotoSans-Regular.ttf",
    "NotoSansArabic-Regular.ttf",
    "NotoSansHebrew-Regular.ttf",
    "NotoSansDevanagari-Regular.ttf",
    "NotoSansBengali-Regular.ttf",
    "NotoSansTamil-Regular.ttf",
    "NotoSansThai-Regular.ttf",
    "NotoSansKhmer-Regular.ttf",
    "NotoSansMyanmar-Regular.ttf",
    "NotoSansArmenian-Regular.ttf",
    "NotoSansGeorgian-Regular.ttf",
    "NotoSansEthiopic-Regular.ttf",
    "NotoSansSC-Regular.otf",
    "NotoSansTC-Regular.otf",
    "NotoSansHK-Regular.otf",
    "NotoSansJP-Regular.otf",
    "NotoSansKR-Regular.otf",
    "NotoSansSymbols2-Regular.ttf",
    "NotoColorEmoji_WindowsCompatible.ttf",
}
actual_files = {entry.get("file") for entry in fonts}
missing_files = sorted(required_files - actual_files)
require(not missing_files, "required global font files are missing: " + ", ".join(missing_files))

fetcher = fetcher_path.read_text(encoding="utf-8")
require("font_manifest.json" in fetcher and "Invoke-WebRequest" in fetcher and "Get-FileHash" in fetcher,
        "PowerShell font fetcher is not manifest-driven and SHA-256 locked")
require("$font.source" in fetcher, "PowerShell font fetcher does not consume pinned manifest URLs")
fetch_wrapper = fetch_wrapper_path.read_text(encoding="utf-8")
require(".git/info/exclude" in fetch_wrapper and "third_party/fonts/noto/*.ttf" in fetch_wrapper,
        "font fetch wrapper must locally exclude binary cache without replacing repository .gitignore")

css = css_path.read_text(encoding="utf-8")
for token in (
    "@font-face", "NotoSansArabic-Regular.ttf", "NotoSansKR-Regular.otf",
    "NotoSansHK-Regular.otf", 'html[dir="rtl"]', 'html[lang^="zh-HK"]',
    'html[lang^="ja"]', 'html[lang^="ko"]', 'html[lang^="ar"]',
):
    require(token in css, f"global WebUI font CSS is missing {token}")

html_files = sorted((ROOT / "src/assets/webui").rglob("*.html"))
require(html_files, "no WebUI HTML files found")
for html in html_files:
    require('/elite_ui.css' in html.read_text(encoding="utf-8"),
            f"WebUI page does not import shared font layer: {html.relative_to(ROOT)}")

languages = json.loads(languages_path.read_text(encoding="utf-8"))
metadata = languages.get("locale_metadata", {})
required_locales = {
    "en": ("ltr", "Latn"),
    "ru": ("ltr", "Cyrl"),
    "uk": ("ltr", "Cyrl"),
    "zh-Hans": ("ltr", "Hans"),
    "zh-Hant": ("ltr", "Hant"),
    "ja": ("ltr", "Jpan"),
    "ko": ("ltr", "Kore"),
    "ar": ("rtl", "Arab"),
    "he": ("rtl", "Hebr"),
    "hi": ("ltr", "Deva"),
    "th": ("ltr", "Thai"),
}
for locale, (direction, script) in required_locales.items():
    entry = metadata.get(locale)
    require(isinstance(entry, dict), f"locale metadata missing for {locale}")
    require(entry.get("direction") == direction and entry.get("script") == script,
            f"locale metadata has wrong direction/script for {locale}")
    require(entry.get("native_name") and entry.get("english_name"),
            f"locale metadata lacks display names for {locale}")

for enabled in languages.get("locale_order", []):
    require(enabled in metadata, f"enabled locale has no metadata: {enabled}")

localization_cpp = (ROOT / "src/game/localization/LocalizationService.cpp").read_text(encoding="utf-8")
i18n_js = (ROOT / "src/assets/webui/game_i18n.js").read_text(encoding="utf-8")
require('fileRoot.contains("locale_metadata")' in localization_cpp and 'root["locale_metadata"]' in localization_cpp,
        "LocalizationService does not parse/emit locale direction metadata")
require("document.documentElement.dir = direction" in i18n_js and "dataset.script" in i18n_js,
        "WebUI i18n runtime does not apply RTL/script metadata to the document")

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
server_cpp = (ROOT / "src/ui/html/HtmlUiServer.cpp").read_text(encoding="utf-8")
server_h = (ROOT / "src/ui/html/HtmlUiServer.h").read_text(encoding="utf-8")
pack_cpp = (ROOT / "src/ui/platform/UiResourcePack.cpp").read_text(encoding="utf-8")
require("build_ui_pack.py" in cmake and "elite_ui.pak" in cmake and "build_ui_pack" in cmake,
        "EliteGame build does not produce the binary UI resource pack")
require("ELITE_REQUIRE_BUNDLED_UI_FONTS" in cmake and "--require-fonts" in cmake,
        "release build has no fail-closed bundled-font gate")
require("UiResourcePack.cpp" in cmake, "UiResourcePack implementation is not part of EliteGame")
require("UiResourcePack" in server_h and "m_resourcePack.read" in server_cpp,
        "HtmlUiServer does not consume the binary UI resource pack")
require(server_cpp.find("m_resourcePack.read") < server_cpp.find("std::filesystem::path fullPath"),
        "HtmlUiServer must serve pack resources before filesystem fallback")
require("ELITEUI1" not in pack_cpp or "kMagic" in pack_cpp,
        "UiResourcePack magic/version validation is missing")

# Exercise the Python pack format independently of the real font cache.
with tempfile.TemporaryDirectory(prefix="elite-ui-pack-") as temp_dir:
    temp = Path(temp_dir)
    webui = temp / "webui"
    fonts_dir = temp / "fonts"
    licenses = temp / "licenses"
    for directory in (webui, fonts_dir, licenses):
        directory.mkdir()
    (webui / "index.html").write_text("<html>ok</html>", encoding="utf-8")
    (webui / "app.css").write_text("body{}", encoding="utf-8")
    (licenses / "NOTICE.txt").write_text("notice", encoding="utf-8")
    mini_manifest = temp / "fonts.json"
    mini_manifest.write_text('{"fonts": []}', encoding="utf-8")
    mini_index = temp / "THIRD_PARTY_LICENSES.md"
    mini_index.write_text("licenses", encoding="utf-8")
    output = temp / "elite_ui.pak"

    subprocess.run(
        [
            sys.executable, str(pack_builder),
            "--webui", str(webui),
            "--fonts", str(fonts_dir),
            "--licenses", str(licenses),
            "--font-manifest", str(mini_manifest),
            "--license-index", str(mini_index),
            "--output", str(output),
        ],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    blob = output.read_bytes()
    require(blob[:8] == b"ELITEUI1", "pack builder emitted wrong binary magic")
    version, count = struct.unpack_from("<II", blob, 8)
    require(version == 1 and count >= 4, "pack builder emitted wrong version/entry count")

print(f"[PASS] global UI font/i18n platform: {len(fonts)} pinned fonts, {len(metadata)} locale metadata records")
