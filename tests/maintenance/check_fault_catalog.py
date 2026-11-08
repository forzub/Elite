"""Gate: the fault catalog (tools/maintenance/build_fault_catalog.py) is consistent and its outputs are current:
every reference resolves, every isolation path concludes its mode, every Cobrett-I unit has a fault class."""
import os
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "maintenance", "build_fault_catalog.py"), "--check"],
                   capture_output=True, text=True, encoding="utf-8", env={**os.environ, "PYTHONIOENCODING": "utf-8"})
sys.stdout.write(r.stdout)
sys.stderr.write(r.stderr)
sys.exit(r.returncode)
