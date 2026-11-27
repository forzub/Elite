"""Audit #7 D7 / HantedPreview: every C++ string literal with Cyrillic text (comments skipped) - a candidate for a
player-visible message built in one language instead of a localized template.

    python tools/preview/find_cyrillic_literals.py [src] [--summary]

Prints file:line: literal. Logs (std::cerr / [Tag] lines) are listed too but marked LOG - they need no translation."""
import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "src"
summary = "--summary" in sys.argv
cyr = re.compile(r"[Ѐ-ӿ]")
lit = re.compile(r'u8?R?"(?:[^"\\\n]|\\.)*"|R"\((?:.|\n)*?\)"')


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif c == "'":
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


per_file = {}
total = 0
logs = 0
for dirpath, _, files in os.walk(root):
    for f in files:
        if not f.endswith((".cpp", ".h", ".hpp")):
            continue
        p = os.path.join(dirpath, f)
        try:
            text = open(p, encoding="utf-8").read()
        except UnicodeDecodeError:
            continue
        code = strip_comments(text)
        lines = code.split("\n")
        for ln, line in enumerate(lines, 1):
            for m in re.finditer(r'"(?:[^"\\]|\\.)*"', line):
                s = m.group(0)
                if cyr.search(s):
                    is_log = "cerr" in line or "cout" in line or "printf" in line or re.search(r'"\[[A-Za-z]', line)
                    total += 1
                    logs += 1 if is_log else 0
                    per_file.setdefault(p, []).append((ln, "LOG " if is_log else "", s[:110]))
for p in sorted(per_file, key=lambda k: -len(per_file[k])):
    items = per_file[p]
    shown = [i for i in items if not i[1]]
    if summary:
        print(f"{len(shown):5d} visible?  {len(items) - len(shown):4d} log  {p}")
    else:
        for ln, tag, s in items:
            print(f"{p}:{ln}: {tag}{s}")
print(f"TOTAL {total} Cyrillic literals, {logs} in log lines, {total - logs} possibly player-visible, {len(per_file)} files")
