"""Compile ONE translation unit with -fsyntax-only, from a ninja compdb (no ninja lock: safe to run in parallel).

    python tools/preview/syntax_check.py <path/to/file.cpp> [compdb.json]

compdb default: D:/tmp/compile_commands.json (made by: ninja -C build-relwithdebinfo -t compdb > D:/tmp/compile_commands.json).
Exit code = the compiler's."""
import json
import os
import re
import shlex
import subprocess
import sys

target = os.path.normcase(os.path.abspath(sys.argv[1])).replace("\\", "/")
db_path = sys.argv[2] if len(sys.argv) > 2 else "D:/tmp/compile_commands.json"
db = json.load(open(db_path, encoding="utf-8"))
for e in db:
    f = e["file"]
    if not os.path.isabs(f):
        f = os.path.join(e["directory"], f)
    if os.path.normcase(os.path.abspath(f)).replace("\\", "/") != target:
        continue
    cmd = e["command"]
    if "c++.exe" not in cmd and "g++" not in cmd and "cc.exe" not in cmd:
        continue
    cmd = re.sub(r"\s-o\s+\S+", " ", cmd)
    cmd = re.sub(r"\s-MD\b|\s-MT\s+\S+|\s-MF\s+\S+", " ", cmd)
    cmd = cmd.replace(" -c ", " -fsyntax-only ")
    env = dict(os.environ)
    env["PATH"] = "C:\\msys64\\mingw64\\bin;" + env.get("PATH", "")
    r = subprocess.run(cmd, cwd=e["directory"], shell=True, env=env)
    sys.exit(r.returncode)
print("not in the compdb: " + target)
sys.exit(2)
