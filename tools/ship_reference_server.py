#!/usr/bin/env python3
"""Local server of the ship reference (Notes/html/ship_reference): the page plus a small API that keeps the
workshop (sketches, notes, model-ready ticks, prompt versions, reviews) in the repository folder
Notes/html/ship_reference/workshop - no browser storage, no folder picker, any browser.

    python tools/ship_reference_server.py            # http://127.0.0.1:8790/ (opens the browser; the next free port if busy)
    python tools/ship_reference_server.py --port 9000 --no-browser

API (JSON unless noted):
    GET    /api/workshop                     -> workshop.json (empty skeleton when missing)
    PUT    /api/workshop                     <- the page's state; MERGED into the file (union by id, the newer
                                                record wins, a record without a note never erases a note), so an
                                                edit made on disk meanwhile (an assistant's review) survives
    POST   /api/file?path=sketches/<part>/<name>   <- raw bytes (an image); written atomically
    DELETE /api/file?path=sketches/<part>/<name>
Static files are served from the reference folder; /workshop/... serves the stored images.
Local only: binds 127.0.0.1.
"""
import argparse
import http.server
import json
import os
import re
import threading
import urllib.parse
import webbrowser

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SITE = os.path.join(ROOT, "Notes", "html", "ship_reference")
WORKSHOP = os.path.join(SITE, "workshop")
STATE = os.path.join(WORKSHOP, "workshop.json")
LOCK = threading.Lock()
SAFE_PATH = re.compile(r"^(sketches|ships)/[A-Za-z0-9._-]+(/[A-Za-z0-9._-]+)?$")
EMPTY = {"version": 1, "modelReady": {}, "prompts": {}, "sketches": {}, "shipImages": {}, "reviews": {}, "deleted": []}


def read_state():
    if not os.path.exists(STATE):
        return json.loads(json.dumps(EMPTY))
    with open(STATE, encoding="utf-8") as f:
        d = json.load(f)
    for k, v in EMPTY.items():
        d.setdefault(k, json.loads(json.dumps(v)))
    return d


def write_atomic(path, data: bytes):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def merge(into, frm):
    """the same rules as store.js merge(): union, the newer record wins, deletions win, notes are kept"""
    for k in ("modelReady", "prompts", "shipImages", "reviews"):
        into.setdefault(k, {})
        for i, v in (frm.get(k) or {}).items():
            cur = into[k].get(i)
            if cur is None or (isinstance(v, dict) and (v.get("updated") or 0) > ((cur or {}).get("updated") or 0)):
                into[k][i] = v
    for pid, lst in (frm.get("sketches") or {}).items():
        mine = into.setdefault("sketches", {}).setdefault(pid, [])
        for s in lst:
            m = next((x for x in mine if x["id"] == s["id"]), None)
            if m is None:
                mine.append(s)
            elif (s.get("updated") or 0) > (m.get("updated") or 0):
                keep = m.get("note") if m.get("note") and not s.get("note") else None
                m.update(s)
                if keep:
                    m["note"] = keep
    gone = set(into.get("deleted") or []) | set(frm.get("deleted") or [])
    into["deleted"] = sorted(gone)
    for pid in list(into.get("sketches", {})):
        into["sketches"][pid] = [s for s in into["sketches"][pid] if s["id"] not in gone]
    return into


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SITE, **kw)

    def log_message(self, fmt, *args):
        if "/api/" in (self.path or ""):
            super().log_message(fmt, *args)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _json(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n) if n else b""

    def _file_path(self):
        q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        rel = (q.get("path") or [""])[0]
        if not SAFE_PATH.match(rel):
            return None
        return os.path.join(WORKSHOP, *rel.split("/"))

    def do_GET(self):
        if self.path.startswith("/api/workshop"):
            with LOCK:
                return self._json(200, read_state())
        if self.path.startswith("/api/ping"):
            return self._json(200, {"ok": True, "workshop": os.path.relpath(WORKSHOP, ROOT)})
        return super().do_GET()

    def do_PUT(self):
        if not self.path.startswith("/api/workshop"):
            return self._json(404, {"error": "not found"})
        try:
            incoming = json.loads(self._body().decode("utf-8"))
        except Exception as e:  # noqa: BLE001
            return self._json(400, {"error": str(e)})
        with LOCK:
            state = merge(read_state(), incoming)
            write_atomic(STATE, json.dumps(state, ensure_ascii=False, indent=1).encode("utf-8"))
            return self._json(200, state)

    def do_POST(self):
        if not self.path.startswith("/api/file"):
            return self._json(404, {"error": "not found"})
        path = self._file_path()
        if not path:
            return self._json(400, {"error": "bad path"})
        data = self._body()
        with LOCK:
            write_atomic(path, data)
        return self._json(200, {"ok": True, "bytes": len(data)})

    def do_DELETE(self):
        if not self.path.startswith("/api/file"):
            return self._json(404, {"error": "not found"})
        path = self._file_path()
        if not path:
            return self._json(400, {"error": "bad path"})
        with LOCK:
            if os.path.exists(path):
                os.remove(path)
        return self._json(200, {"ok": True})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8790)
    ap.add_argument("--no-browser", action="store_true")
    a = ap.parse_args()
    os.makedirs(WORKSHOP, exist_ok=True)
    srv = None
    for port in range(a.port, a.port + 20):   # another local service may hold the port
        try:
            srv = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
            break
        except OSError:
            continue
    if srv is None:
        raise SystemExit(f"no free port in {a.port}..{a.port + 19}")
    url = f"http://127.0.0.1:{srv.server_address[1]}/"
    print(f"ship reference: {url}  (workshop: {os.path.relpath(WORKSHOP, ROOT)}); Ctrl+C stops")
    if not a.no_browser:
        threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
