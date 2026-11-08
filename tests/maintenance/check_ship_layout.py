"""Gate: the fault catalog (tools/maintenance/build_ship_layout.py) is consistent and its outputs are current:
unique designations, every BOM unit placed."""
import os
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "maintenance", "build_ship_layout.py"), "--check"],
                   capture_output=True, text=True, encoding="utf-8", env={**os.environ, "PYTHONIOENCODING": "utf-8"})
sys.stdout.write(r.stdout)
sys.stderr.write(r.stderr)
sys.exit(r.returncode)
