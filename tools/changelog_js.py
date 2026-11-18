#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""CHANGELOG.md -> web/changelog.js, the change log the manual shows (manual.html#changelog).

  tools/changelog_js.py [CHANGELOG.md] [web/changelog.js]

CHANGELOG.md stays the one source: "## YYYY-MM-DD: a title" a day (the title optional), "- **Kind:** text" a line
(continued on lines indented two spaces). Each kind falls in one of the filters below; an unknown kind stops the build,
so every line keeps a kind the page can filter. A link to a repository file becomes its GitHub address."""
import json
import re
import sys

REPO = "https://github.com/InigoHernandez/affine/blob/main/"
KINDS = {"release": "Releases", "play": "Playing", "sound": "Sound", "screen": "Screen", "fixes": "Fixes",
         "web": "Website", "docs": "Docs"}
GROUP = {"release": "release", "hub": "docs", "docs": "docs", "perform": "play", "controls": "play", "sound": "sound",
         "breaks": "sound", "fork": "sound", "screen": "screen", "fix": "fixes", "safety": "fixes", "site": "web",
         "installer": "web"}


def parse(md):
    days, day, item = [], None, None
    for raw in md.splitlines():
        line = raw.rstrip()
        m = re.match(r"## (\d{4}-\d{2}-\d{2})(?::\s*(.+))?$", line)
        if m:
            day = {"date": m.group(1), "title": m.group(2) or "", "items": []}
            days.append(day)
            item = None
            continue
        m = re.match(r"- \*\*([^*]+?):\*\*\s*(.*)$", line)
        if m and day is not None:
            kind = m.group(1).strip()
            ver = re.match(r"Release (\d+(?:\.\d+)+)$", kind)
            key = "release" if ver else kind.lower()
            if key not in GROUP:
                raise SystemExit(f"changelog_js: the kind {kind!r} ({day['date']}) has no filter: add it to GROUP")
            item = [GROUP[key], (f"{ver.group(1)}: " if ver else "") + m.group(2)]
            day["items"].append(item)
            continue
        if line.startswith("  ") and item is not None:
            item[1] += " " + line.strip()
            continue
        if line.strip() == "":
            item = None
    for d in days:
        for it in d["items"]:
            t = re.sub(r"\*\*(.+?)\*\*", r"\1", it[1])           # (bold: plain on the page)
            t = re.sub(r"\]\((?!https?:)([^)]+)\)", lambda m: "](" + REPO + m.group(1) + ")", t)
            it[1] = t[:1].upper() + t[1:]                        # (after "**Kind:**" a line starts small)
    return days


def main(src="CHANGELOG.md", dst="web/changelog.js"):
    days = parse(open(src, encoding="utf-8").read())
    if not days:
        raise SystemExit("changelog_js: no days found")
    out = ("// SPDX-License-Identifier: MPL-2.0\n"
           "// GENERATED from CHANGELOG.md by tools/changelog_js.py (web/build.sh runs it): edit CHANGELOG.md, not this.\n"
           f"window.AFF_KINDS = {json.dumps(KINDS, ensure_ascii=False)};\n"
           f"window.AFF_CHANGES = {json.dumps(days, ensure_ascii=False, indent=1)};\n")
    open(dst, "w", encoding="utf-8").write(out)
    print(f"changelog_js: {sum(len(d['items']) for d in days)} lines, {len(days)} days -> {dst}")


if __name__ == "__main__":
    main(*sys.argv[1:3])
