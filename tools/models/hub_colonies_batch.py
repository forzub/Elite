"""Batch of colony hubs (tools/models/hub_colonies.py) for the whole inhabited map.

  python tools/models/hub_colonies_batch.py --missing [--jobs 3]     every site hub_coverage.py lists as buildable
  python tools/models/hub_colonies_batch.py <site id> [...]

The profile / world entry / city attach steps write shared files (hub_profiles.json, initial_world_state.json,
hub_semantic_anchors.json, ...) and run one hub at a time; the frozen layouts (one file per hub) run aside in --jobs
parallel workers. A site whose world entry and layout both exist is skipped (resumable).
"""
import concurrent.futures
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)


def run(args):
    r = subprocess.run([sys.executable] + args, cwd=ROOT, stdin=subprocess.DEVNULL, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    return r.returncode, (r.stdout + r.stderr).strip()


def main():
    sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)
    args = sys.argv[1:]
    jobs = int(args[args.index("--jobs") + 1]) if "--jobs" in args else 3
    import hub_coverage
    _, sites, hubs, layouts = hub_coverage.load()
    if "--missing" in args:
        ids = [s["id"] for s in sites if (s.get("population") or 0) > 0 and hub_coverage.buildable(s, hubs) is None]
    else:
        ids = [a for a in args if not a.startswith("--") and not a.isdigit()]
    todo = [i for i in ids if not (i in hubs and i in layouts)]
    print(f"{len(todo)} hubs to build ({len(ids) - len(todo)} done already), layouts in {jobs} workers")
    t0 = time.time()
    failed = []
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        futs = {}
        for n, sid in enumerate(todo):
            code, out = run([os.path.join(HERE, "hub_colonies.py"), sid, "--no-layout"])
            if code:
                failed.append(sid)
                print(f"[{n + 1}/{len(todo)}] {sid}: FAILED\n{out[-1500:]}")
                continue
            print(f"[{n + 1}/{len(todo)}] {time.time() - t0:.0f} s  " + out.splitlines()[-1].strip())
            futs[pool.submit(run, [os.path.join(HERE, "hub_city_layout.py"), sid])] = sid
        for f in concurrent.futures.as_completed(futs):
            code, out = f.result()
            if code:
                failed.append(futs[f])
                print(f"layout {futs[f]}: FAILED\n{out[-1500:]}")
    for f in futs:
        code, out = f.result()
        if not code:
            print("  " + out.splitlines()[-1])
    print(f"done in {time.time() - t0:.0f} s; failed {len(failed)}: {' '.join(failed)}")


if __name__ == "__main__":
    main()
