"""Static HTML mirror of Notes/Lore -> Notes/html (deterministic).

Markdown stays the source; this only renders it for local reading. Run after
lore_md.py --write (or after any hand edit of Notes/Lore):

    python lore_html.py            # write Notes/html (stale pages of deleted .md are removed)
    python lore_html.py --check    # compare with the committed mirror, no writes

Needs markdown-it-py (CommonMark + tables). GENERATED markers stay HTML
comments (invisible); links to .md become .html; every table is wrapped for
horizontal scrolling; headings get stable ids.
"""
import argparse
import glob
import html
import os
import posixpath
import re
import sys

from markdown_it import MarkdownIt

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(ROOT, "Notes", "Lore")
DST = os.path.join(ROOT, "Notes", "html")

CSS = """body{margin:0;font:16px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:#f7f7f4;color:#1d1f21}
header{padding:10px 16px;background:#20242a;color:#ddd}header a{color:#9fd3ff}
.wrap{max-width:1180px;margin:0 auto;padding:16px}
h1,h2,h3{line-height:1.25}h1{font-size:1.7em}
a{color:#0b62a3}code{background:#ecece6;padding:1px 4px;border-radius:3px;font-size:.92em}
pre{background:#ecece6;padding:10px;overflow:auto}
blockquote{margin:0;padding:6px 14px;border-left:4px solid #c9c9bd;background:#efefe9}
.table-wrap{overflow-x:auto;margin:10px 0}table{border-collapse:collapse;font-size:.92em}
th,td{border:1px solid #d3d3c8;padding:4px 8px;vertical-align:top}th{background:#e7e7df;text-align:left}
.small{font-size:.85em;color:#666}.path{margin-top:24px;font-size:.85em;color:#666}
.index-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:12px}
.card{background:#fff;border:1px solid #ddd;border-radius:6px;padding:8px 14px}.card ul{padding-left:18px}
@media (prefers-color-scheme:dark){body{background:#16181b;color:#ddd}a{color:#8cc8ff}code,pre,blockquote{background:#23262b}
th{background:#262a30}th,td{border-color:#3a3f46}.card{background:#1d2024;border-color:#33373d}.small,.path{color:#999}}
"""

MD = MarkdownIt("commonmark", {"html": True}).enable("table")
LINK_RE = re.compile(r'href="([^"#:]+?)\.md(#[^"]*)?"')


def slug(text):
    s = re.sub(r"<[^>]+>", "", text).strip().lower()
    s = re.sub(r"[^\w\s-]", "", s, flags=re.UNICODE)
    return re.sub(r"\s+", "-", s)


def heading_ids(body):
    seen = {}

    def rep(m):
        lvl, inner = m.group(1), m.group(2)
        sid = slug(html.unescape(inner)) or "section"
        n = seen.get(sid, 0)
        seen[sid] = n + 1
        if n:
            sid = f"{sid}-{n}"
        return f'<h{lvl} id="{html.escape(sid)}">{inner}</h{lvl}>'
    return re.sub(r"<h([1-6])>(.*?)</h\1>", rep, body)


def page(rel_md, text):
    depth = rel_md.count("/")
    up = "../" * depth
    body = MD.render(text)
    body = LINK_RE.sub(lambda m: f'href="{m.group(1)}.html{m.group(2) or ""}"', body)
    body = body.replace("<table>", '<div class="table-wrap"><table>').replace("</table>", "</table></div>")
    body = heading_ids(body)
    m = re.search(r"^#\s+(.+)$", text, flags=re.M)
    title = html.escape(m.group(1).strip()) if m else html.escape(rel_md)
    return ('<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
            f'<title>{title} — Elite Lore</title><link rel="stylesheet" href="{up}style.css"></head><body><header><a href="{up}index.html">Оглавление</a></header>'
            f'<main class="wrap"><article id="content">\n{body}</article><footer class="path">Источник: <code>Notes/Lore/{html.escape(rel_md)}</code></footer></main></body></html>\n')


def index(rels):
    groups = {}
    for r in rels:
        groups.setdefault(r.split("/")[0] if "/" in r else "", []).append(r)
    cards = []
    for g in sorted(groups):
        items = "".join(f'<li><a href="{r[:-3]}.html">{html.escape(posixpath.basename(r)[:-3].replace("_", " "))}</a><div class="small">{html.escape(r)}</div></li>'
                        for r in sorted(groups[g]))
        cards.append(f'<section class="card"><h2>{html.escape(g or "Lore")}</h2><ul>{items}</ul></section>')
    return ('<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
            '<title>Elite Lore — HTML</title><link rel="stylesheet" href="style.css"></head><body><header><span class="small">Статическая HTML-копия Notes/Lore</span></header>'
            '<main class="wrap"><h1>Elite — Lore</h1><p>HTML-копия <code>Notes/Lore/</code> для локального чтения. Исходником остаётся Markdown; '
            'пересборка — <code>python tools/colonization_lab/lore_html.py</code>.</p>'
            f'<p class="small">Документов: {len(rels)}.</p><div class="index-grid">{"".join(cards)}</div></main></body></html>\n')


def build():
    rels = sorted(posixpath.relpath(p.replace(os.sep, "/"), SRC.replace(os.sep, "/")) for p in glob.glob(os.path.join(SRC, "**", "*.md"), recursive=True))
    files = {"style.css": CSS, "index.html": index(rels)}
    for r in rels:
        with open(os.path.join(SRC, r), encoding="utf-8") as f:
            files[r[:-3] + ".html"] = page(r, f.read())
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    files = build()
    existing = {posixpath.relpath(p.replace(os.sep, "/"), DST.replace(os.sep, "/")) for p in glob.glob(os.path.join(DST, "**", "*"), recursive=True) if os.path.isfile(p)}
    stale = sorted(existing - set(files))
    if a.check:
        bad = [r for r, t in files.items() if not os.path.isfile(os.path.join(DST, r)) or open(os.path.join(DST, r), encoding="utf-8").read() != t]
        for r in bad[:20]:
            print(f"STALE: Notes/html/{r}")
        for r in stale[:20]:
            print(f"ORPHAN: Notes/html/{r}")
        ok = not bad and not stale
        print(f"lore_html check {'OK' if ok else 'FAILED'} ({len(files)} files)")
        return 0 if ok else 1
    for r, t in files.items():
        p = os.path.join(DST, r)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(t)
    for r in stale:
        os.remove(os.path.join(DST, r))
    print(f"lore_html: {len(files)} files written, {len(stale)} stale removed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
