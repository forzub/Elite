"""The vendored third_party sources: a manifest with versions, sources and content hashes (audit #9 P0, the program
stage 0 / D6).

    python tools/third_party_manifest.py write    -> third_party/MANIFEST.json + tools/third_party_manifest.json (tracked)
    python tools/third_party_manifest.py check    -> exit 1 when a folder is missing / empty / its hash differs

The hash is a sha256 over every file's relative path and bytes (sorted), so a partial or edited folder is caught."""
import hashlib
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TP = os.path.join(ROOT, "third_party")
TRACKED = os.path.join(ROOT, "tools", "third_party_manifest.json")
DEPS = {
    "asio": {"version": "1.28.2", "source": "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-28-2.tar.gz",
             "note": "chosen 2026-11-27 (the pre-loss version unknown): the last release with io_service for websocketpp 0.8.2"},
    "websocketpp": {"version": "0.8.2", "source": "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz"},
    "webview": {"version": "0.12.0", "source": "https://github.com/webview/webview/archive/refs/tags/0.12.0.tar.gz",
                "note": "its default WebView2 SDK (1.0.1150.38) matches the old build cache"},
    "miniaudio": {"version": "0.11.21", "source": "https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz"},
    "webview2": {"version": "1.0.1150.38", "source": "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.1150.38",
                 "note": "not referenced by CMake (the build fetches its own copy into _deps)"},
}


def tree_hash(path):
    h = hashlib.sha256()
    files = 0
    for dirpath, dirs, names in os.walk(path):
        dirs.sort()
        for n in sorted(names):
            p = os.path.join(dirpath, n)
            rel = os.path.relpath(p, path).replace("\\", "/")
            h.update(rel.encode("utf-8") + b"\0")
            with open(p, "rb") as f:
                h.update(f.read())
            files += 1
    return h.hexdigest(), files


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    if mode == "write":
        out = {}
        for name, meta in DEPS.items():
            digest, files = tree_hash(os.path.join(TP, name))
            out[name] = dict(meta, sha256_tree=digest, files=files)
        json.dump(out, open(TRACKED, "w", encoding="utf-8"), indent=1)
        print(json.dumps({k: (v["files"], v["sha256_tree"][:16]) for k, v in out.items()}))
        return 0
    expected = json.load(open(TRACKED, encoding="utf-8"))
    bad = 0
    for name, meta in expected.items():
        p = os.path.join(TP, name)
        if not os.path.isdir(p) or not os.listdir(p):
            print(f"MISSING {name} ({meta['version']}) - run tools/fetch_third_party.sh")
            bad += 1
            continue
        digest, files = tree_hash(p)
        if digest != meta["sha256_tree"]:
            print(f"DIFFERS {name}: {files} files, hash {digest[:16]} != {meta['sha256_tree'][:16]}")
            bad += 1
        else:
            print(f"ok {name} {meta['version']} ({files} files)")
    return 1 if bad else 0


sys.exit(main())
