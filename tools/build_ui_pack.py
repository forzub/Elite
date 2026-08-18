#!/usr/bin/env python3
"""Build the deterministic binary resource pack used by the embedded Elite WebUI."""

import argparse
import json
import mimetypes
import struct
from pathlib import Path

MAGIC = b"ELITEUI1"
VERSION = 1
FONT_EXTENSIONS = {".ttf", ".otf", ".woff2"}


def add_item(items, resource, path):
    path = Path(path)
    if not path.is_file():
        return
    if resource in items:
        raise RuntimeError(f"duplicate UI-pack resource: {resource}")
    items[resource] = path


def collect(webui, fonts, licenses, font_manifest=None, license_index=None):
    items = {}

    webui = Path(webui)
    for path in sorted(webui.rglob("*")):
        if path.is_file():
            add_item(items, "/" + path.relative_to(webui).as_posix(), path)

    fonts = Path(fonts)
    if fonts.exists():
        for path in sorted(fonts.rglob("*")):
            if path.is_file() and path.suffix.lower() in FONT_EXTENSIONS:
                add_item(items, "/ui/fonts/" + path.relative_to(fonts).as_posix(), path)
        font_lock = fonts / "font-lock.json"
        if font_lock.is_file():
            add_item(items, "/ui/font-lock.json", font_lock)

    licenses = Path(licenses)
    if licenses.exists():
        for path in sorted(licenses.rglob("*")):
            if path.is_file():
                add_item(items, "/licenses/" + path.relative_to(licenses).as_posix(), path)

    if font_manifest:
        add_item(items, "/ui/font_manifest.json", font_manifest)
    if license_index:
        add_item(items, "/licenses/THIRD_PARTY_LICENSES.md", license_index)

    return sorted(items.items())


def mime(path):
    ext = Path(path).suffix.lower()
    return {
        ".css": "text/css; charset=utf-8",
        ".js": "application/javascript; charset=utf-8",
        ".html": "text/html; charset=utf-8",
        ".json": "application/json; charset=utf-8",
        ".ttf": "font/ttf",
        ".otf": "font/otf",
        ".woff2": "font/woff2",
        ".txt": "text/plain; charset=utf-8",
        ".md": "text/markdown; charset=utf-8",
    }.get(ext, mimetypes.guess_type(path)[0] or "application/octet-stream")


def expected_font_files(manifest_path):
    if not manifest_path:
        return []
    data = json.loads(Path(manifest_path).read_text(encoding="utf-8"))
    return [entry["file"] for entry in data.get("fonts", [])]


def build(args):
    expected = expected_font_files(args.font_manifest)
    font_root = Path(args.fonts)
    missing = [name for name in expected if not (font_root / name).is_file()]
    if missing:
        message = (
            f"{len(missing)} bundled UI fonts are missing; "
            "run tools/fetch_ui_fonts.ps1 before a release build"
        )
        if args.require_fonts:
            raise RuntimeError(message + ": " + ", ".join(missing))
        print(f"[UI-PACK][WARN] {message}")

    items = collect(
        args.webui,
        args.fonts,
        args.licenses,
        args.font_manifest,
        args.license_index,
    )

    entries = []
    index_size = 0
    for resource, path in items:
        resource_bytes = resource.encode("utf-8")
        mime_bytes = mime(resource).encode("utf-8")
        data = path.read_bytes()
        if len(resource_bytes) > 0xFFFF or len(mime_bytes) > 0xFFFF:
            raise RuntimeError(f"UI-pack index string is too long: {resource}")
        index_size += 2 + 2 + 8 + 8 + len(resource_bytes) + len(mime_bytes)
        entries.append((resource_bytes, mime_bytes, data))

    payload_offset = len(MAGIC) + 4 + 4 + index_size
    cursor = payload_offset
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)

    with output.open("wb") as stream:
        stream.write(MAGIC)
        stream.write(struct.pack("<II", VERSION, len(entries)))
        for resource_bytes, mime_bytes, data in entries:
            stream.write(
                struct.pack(
                    "<HHQQ",
                    len(resource_bytes),
                    len(mime_bytes),
                    cursor,
                    len(data),
                )
            )
            stream.write(resource_bytes)
            stream.write(mime_bytes)
            cursor += len(data)
        for _, _, data in entries:
            stream.write(data)

    print(
        f"[UI-PACK] {output} entries={len(entries)} "
        f"bytes={output.stat().st_size} fonts={len(expected) - len(missing)}/{len(expected)}"
    )


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--webui", required=True)
    parser.add_argument("--fonts", required=True)
    parser.add_argument("--licenses", required=True)
    parser.add_argument("--font-manifest")
    parser.add_argument("--license-index")
    parser.add_argument("--require-fonts", action="store_true")
    parser.add_argument("--output", required=True)
    return parser.parse_args()


if __name__ == "__main__":
    build(parse_args())
