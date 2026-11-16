#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AFFINE: fm1-base's installer page (tools/make_site.py's install/index.html) in the site's look: POLYGON's colours,
the pixel font for titles and buttons, square corners, a bar back to the site, and a line saying whether the package
has run on an FM-1. The page's own text, steps and scripts stay fm1-base's.
  tools/install_theme.py IN_HTML OUT_HTML [--untested]"""
import sys

STYLE = """<link rel="icon" href="../gem.svg" type="image/svg+xml">
<style>/* AFFINE (tools/install_theme.py): the installer in POLYGON */
@font-face { font-family: "AFF Pixel"; src: url(../fonts/mono2k-font.otf) format("opentype"); font-display: block; }
@font-face { font-family: "Inter Tight"; src: url(../fonts/InterTight-400.woff) format("woff"); font-weight: 400; }
@font-face { font-family: "Inter Tight"; src: url(../fonts/InterTight-600.woff) format("woff"); font-weight: 600 900; }
:root, :root:not([data-theme="light"]), :root[data-theme="dark"] {
  --bg: #060814; --fg: #ece6d6; --dim: #9896b4; --accent: #ff5c28; --on-accent: #060814; --bad: #ff8a73; --focus: #46d6c8;
  --line: #282454; --card: #181634; color-scheme: dark; }
html { background: var(--bg); }
body { font: 17px/1.5 "Inter Tight", "Helvetica Neue", Arial, sans-serif;
  background-image: repeating-linear-gradient(90deg, transparent 0 calc(100% / 12 - 1px), color-mix(in srgb, var(--line) 70%, transparent) calc(100% / 12 - 1px) calc(100% / 12)); }
main { max-width: 600px; padding-top: 32px; }
footer { max-width: 600px; }
h1 { font: 400 48px/1 "AFF Pixel", ui-monospace, monospace; text-transform: uppercase; margin: 0 0 24px;
  display: flex; align-items: center; gap: 16px; }
h1::before { content: ""; flex: 0 0 auto; width: 40px; height: 40px; transform: skewX(-31deg);
  background: linear-gradient(90deg, var(--line) 0 18%, transparent 18% 27%, #545082 27% 45%, transparent 45% 54%, #a8462a 54% 72%, transparent 72% 81%, var(--accent) 81%); }
h2 { font: 400 24px/1.1 "AFF Pixel", ui-monospace, monospace; text-transform: uppercase; padding-bottom: 8px; border-bottom: 3px solid var(--accent); }
button { border-radius: 0; font: 400 16px/1 "AFF Pixel", ui-monospace, monospace; text-transform: uppercase; padding: 12px 20px; }
button:hover:not(:disabled) { outline: 2px solid var(--accent); outline-offset: 2px; }
button.plain { border-color: var(--line); }
.lang { font: 400 16px/1 "AFF Pixel", ui-monospace, monospace; text-decoration: none; color: var(--dim); border: 1px solid var(--line); padding: 6px 8px; }
#go { font-size: 32px; padding: 20px 32px; }
progress { height: 12px; accent-color: var(--accent); }
#log, textarea { border-radius: 0; border-color: var(--line); background: var(--card); }
.backup { border-radius: 0; }
dialog { border-radius: 0; border: 1px solid var(--accent); }
.affbar { display: flex; justify-content: space-between; align-items: center; gap: 16px; min-height: 48px; padding: 0 16px;
  border-bottom: 1px solid var(--line); background: var(--bg); font: 400 16px/1 "AFF Pixel", ui-monospace, monospace; text-transform: uppercase; }
.affbar a { display: inline-flex; align-items: center; gap: 8px; text-decoration: none; }
.affbar a:hover { color: var(--accent); }
.affbar svg { width: 24px; height: 24px; }
.affbar span { color: var(--dim); }
.affnote { padding: 12px 14px; border: 1px solid var(--focus); color: var(--fg); }
.affnote b { font: 400 16px/1 "AFF Pixel", ui-monospace, monospace; text-transform: uppercase; color: var(--focus); }
</style>
"""
BAR = """<nav class="affbar" aria-label="AFFINE"><a href="../"><svg viewBox="0 0 32 32" aria-hidden="true" shape-rendering="crispEdges"><path fill="#ff5c28" d="M16 1 4 13h12z"/><path fill="#f6a382" d="M16 1 28 13H16z"/><path fill="#8a3a22" d="M4 13h12v18z"/><path fill="#4a2018" d="M16 13h12L16 31z"/></svg>AFFINE</a><span>Install</span></nav>
"""
NOTE = """  <p class="affnote"><b>First release.</b> This build has not run on an FM-1 yet. If it misbehaves, the same page puts M-VAVE's firmware back (below).</p>
"""


def main(src, dst, untested):
    html = open(src, encoding="utf-8").read()
    for mark in ("</head>", "<body>\n", '  <p data-t="warn">'):
        if html.count(mark) != 1:
            raise SystemExit(f"install_theme: {mark!r} not found once in {src}: fm1-base's installer changed")
    html = html.replace("</head>", STYLE + "</head>", 1).replace("<body>\n", "<body>\n" + BAR, 1)
    if untested:
        html = html.replace('  <p data-t="warn">', NOTE + '  <p data-t="warn">', 1)
    open(dst, "w", encoding="utf-8").write(html)


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        sys.exit(__doc__)
    main(args[0], args[1], "--untested" in sys.argv)
