"""Compare two hub layout dumps (hub_map_dump.py): python tools/models/hub_map_compare.py a.json b.json
The access bridges (seeded per street since the hub map) are compared by count only; node ids are ignored."""
import collections
import json
import re
import sys


def key_nodes(L):
    out, acc = collections.Counter(), 0
    for nid, folder, pos, rot, role, parent in L["nodes"]:
        if role == "access" or re.search(r"_u?ag-?\d", nid):
            acc += 1
            continue
        out[(folder, tuple(round(v, 1) for v in pos), tuple(round(v, 2) for v in rot), role)] += 1
    return out, acc


def main():
    a, b = (json.load(open(p, encoding="utf-8")) for p in sys.argv[1:3])
    na, aa = key_nodes(a)
    nb, ab = key_nodes(b)
    bad = 0
    if na != nb:
        bad += 1
        print("nodes differ:", sum((na - nb).values()), "only in a,", sum((nb - na).values()), "only in b")
        for k in list((na - nb).elements())[:6]:
            print("  a:", k)
        for k in list((nb - na).elements())[:6]:
            print("  b:", k)
    print(f"nodes {sum(na.values())} / {sum(nb.values())}, access {aa} / {ab}")
    for k in ("spinning", "haze", "docks", "lanes", "connectors", "red_zones"):
        same = json.dumps(a[k], sort_keys=True) == json.dumps(b[k], sort_keys=True)
        print(f"{k}: {'same' if same else 'DIFFERENT'} ({len(a[k])} / {len(b[k])})")
        bad += not same
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
