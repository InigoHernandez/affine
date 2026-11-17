#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AFFINE: fm1-base's installer page (tools/make_site.py's install/index.html) in the site's look: POLYGON's colours,
the pixel font for titles and buttons, square corners, a bar back to the site, and a line saying whether the package
has run on an FM-1. The page's own text, steps and scripts stay fm1-base's.
  tools/install_theme.py IN_HTML OUT_HTML [--untested]"""
import sys

STYLE = """<link rel="icon" href="../gem.svg" type="image/svg+xml">
<style>/* AFFINE (tools/install_theme.py): the installer on the site's system (web/style.css): POLYGON's colours, the 8 px
 * scale, the 12-column rules, one button (primary, outline; 32 / 40 / 48), cards, a keycap-free page; motion from
 * transitions.dev's tokens (the dialog opens like its modal), the Install button's beam after Libraries.dev's */
@font-face { font-family: "AFF Pixel"; src: url(../fonts/mono2k-font.otf) format("opentype"); font-display: block; }
@font-face { font-family: "Inter Tight"; src: url(../fonts/InterTight-400.woff) format("woff"); font-weight: 400; }
@font-face { font-family: "Inter Tight"; src: url(../fonts/InterTight-600.woff) format("woff"); font-weight: 600 900; }
:root, :root:not([data-theme="light"]), :root[data-theme="dark"] {
  --bg: #060814; --fg: #ece6d6; --dim: #9896b4; --accent: #ff5c28; --on-accent: #060814; --bad: #ff8a73; --focus: #ff5c28;
  --line: #282454; --card: #181634; --cool: #46d6c8; color-scheme: dark;
  --pix: "AFF Pixel", ui-monospace, Menlo, monospace; --label: 400 16px / 20px var(--pix);
  --s05: 4px; --s1: 8px; --s15: 12px; --s2: 16px; --s3: 24px; --s4: 32px; --s6: 48px; --gut: 16px; --max: 1440px;
  --pad: max(var(--gut), calc((100% - var(--max)) / 2 + var(--gut)));
  --h-sm: 32px; --h-md: 40px; --h-lg: 48px;
  --duration-quick: 150ms; --duration-fast: 250ms; --ease-smooth-out: cubic-bezier(0.22, 1, 0.36, 1); }
html { background: var(--bg); overflow-x: clip; }
[hidden] { display: none !important; }   /* (the page hides its buttons with the attribute; a button's own display must not win) */
body { font: 17px/1.5 "Inter Tight", "Helvetica Neue", Arial, sans-serif; -webkit-font-smoothing: antialiased; background: transparent; }
.affrules { position: fixed; inset: 0; z-index: -1; pointer-events: none; display: grid; grid-template-columns: repeat(12, minmax(0, 1fr));
  column-gap: var(--gut); padding: 0 var(--pad); }
.affrules i { border-left: 1px dotted var(--line); }
.affrules i:last-child { border-right: 1px dotted var(--line); }
main, footer { max-width: 640px; padding-left: var(--gut); padding-right: var(--gut); }
main { padding-top: var(--s6); }
main > p, main > div, main > ol { margin: 0 0 var(--s2); }
h1 { font: 400 48px/1 var(--pix); text-transform: uppercase; margin: 0 0 var(--s3); display: flex; align-items: center; gap: var(--s2); }
h1::before { content: ""; flex: 0 0 auto; width: 40px; height: 40px; transform: skewX(-31deg);
  background: linear-gradient(90deg, var(--line) 0 18%, transparent 18% 27%, #545082 27% 45%, transparent 45% 54%, #a8462a 54% 72%, transparent 72% 81%, var(--accent) 81%); }
h2 { font: 400 24px/1.1 var(--pix); text-transform: uppercase; margin: var(--s6) 0 var(--s2); padding-bottom: var(--s1); border-bottom: 3px solid var(--accent); }
.small { font-size: 14px; line-height: 20px; }
/* buttons: primary (hot), .plain outline; 40 high, Install 48 */
button { display: inline-flex; align-items: center; justify-content: center; height: var(--h-md); padding: 0 var(--s2); border-radius: 0;
  font: var(--label); text-transform: uppercase; color: var(--on-accent); background: var(--accent); border: 1px solid var(--accent);
  transition: background-color var(--duration-quick) var(--ease-smooth-out), color var(--duration-quick) var(--ease-smooth-out); }
button:hover:not(:disabled) { background: var(--bg); color: var(--accent); }
button:active:not(:disabled) { transform: translateY(1px); }
button:disabled { opacity: 0.5; }
button.plain { color: var(--fg); background: var(--bg); border-color: var(--line); }
button.plain:hover:not(:disabled) { color: var(--fg); border-color: var(--fg); }
:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
.lang { position: absolute; top: var(--s1); right: var(--pad); height: var(--h-sm); padding: 0 var(--s15); float: none;
  font: 400 14px/1 "Inter Tight", sans-serif; text-transform: none; text-decoration: none; color: var(--dim); background: var(--bg); border: 1px solid var(--line); }
.lang:hover:not(:disabled) { color: var(--fg); background: var(--bg); border-color: var(--fg); }
#go { position: relative; height: var(--h-lg); padding: 0 var(--s4); font-size: 32px; }
/* the beam round Install, in pixel steps (once it can be pressed) */
@property --beam { syntax: "<angle>"; inherits: false; initial-value: 0deg; }
#go:not(:disabled)::before { content: ""; position: absolute; inset: -4px; padding: 3px; pointer-events: none;
  background: conic-gradient(from var(--beam), transparent 0 72%, color-mix(in srgb, var(--fg) 35%, transparent) 84%, var(--fg) 92%, transparent 93%);
  -webkit-mask: linear-gradient(#000 0 0) content-box, linear-gradient(#000 0 0); -webkit-mask-composite: xor;
  mask: linear-gradient(#000 0 0) content-box exclude, linear-gradient(#000 0 0); animation: beam 2.8s steps(56) infinite; }
@keyframes beam { to { --beam: 360deg; } }
input[type=file] { font-size: 14px; color: var(--dim); }
input[type=file]::file-selector-button { height: var(--h-sm); margin-right: var(--s1); padding: 0 var(--s15); font: var(--label); text-transform: uppercase;
  color: var(--fg); background: var(--bg); border: 1px solid var(--line); border-radius: 0; cursor: pointer; }
input[type=file]::file-selector-button:hover { border-color: var(--fg); }
input[type=checkbox] { accent-color: var(--accent); width: 16px; height: 16px; }
progress { appearance: none; -webkit-appearance: none; height: 8px; border: 1px solid var(--line); background: var(--card); }
progress::-webkit-progress-bar { background: var(--card); }
progress::-webkit-progress-value { background: var(--accent); transition: width var(--duration-fast) var(--ease-smooth-out); }
progress::-moz-progress-bar { background: var(--accent); }
#log, textarea { border-radius: 0; border: 1px solid var(--line); background: var(--card); }
/* cards: the note, the backup, the dialogs */
.affnote, .backup { padding: var(--s15) var(--s2); border: 1px solid var(--line); border-radius: 0; background: var(--bg); }
.affnote { border-color: color-mix(in srgb, var(--cool) 45%, var(--line)); }
.affnote b { font: var(--label); text-transform: uppercase; color: var(--cool); margin-right: var(--s05); }
.backup { border-color: var(--accent); }
dialog { border-radius: 0; border: 1px solid var(--line); background: var(--card); padding: var(--s3); box-shadow: none; }
dialog h2 { margin-top: 0; }
dialog .row { gap: var(--s1); }
dialog[open] { animation: aff-modal var(--duration-fast) var(--ease-smooth-out); }
dialog[open]::backdrop { background: rgba(6, 8, 20, 0.7); animation: aff-fade var(--duration-fast) var(--ease-smooth-out); }
@keyframes aff-modal { from { opacity: 0; scale: 0.96; } }
@keyframes aff-fade { from { opacity: 0; } }
/* the bar: the site's header */
.affbar { position: relative; display: flex; justify-content: space-between; align-items: center; gap: var(--s2); min-height: var(--h-lg);
  padding: 0 var(--pad); border-bottom: 1px solid var(--line); background: var(--bg); font: 400 20px/1 var(--pix); text-transform: uppercase; }
.affbar a { display: inline-flex; align-items: center; gap: var(--s1); text-decoration: none; }
.affbar a:hover { color: var(--accent); }
.affbar svg { width: 24px; height: 24px; }
footer { padding-bottom: var(--s6); }
@media (max-width: 480px) { h1 { font-size: 32px; gap: 12px; } h1::before { width: 28px; height: 28px; } #go { font-size: 24px; } }   /* (one line on a phone) */
@media (max-width: 360px) { h1 { font-size: 24px; gap: 10px; } h1::before { width: 22px; height: 22px; } }
@media (prefers-reduced-motion: reduce) { #go::before { animation: none; } dialog[open], dialog[open]::backdrop { animation: none; } }
</style>
"""
BAR = """<div class="affrules" aria-hidden="true"><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></div>
<nav class="affbar" aria-label="AFFINE"><a href="../"><svg viewBox="0 0 32 32" aria-hidden="true" shape-rendering="crispEdges"><path fill="#ff5c28" d="M16 1 4 13h12z"/><path fill="#f6a382" d="M16 1 28 13H16z"/><path fill="#8a3a22" d="M4 13h12v18z"/><path fill="#4a2018" d="M16 13h12L16 31z"/></svg>AFFINE</a></nav>
"""
NOTE = """  <p class="affnote"><b>Beta.</b> This build has not run on an FM-1 yet. If it misbehaves, the same page puts M-VAVE's firmware back (below).</p>
"""

# the footer: AFFINE's credits once (fm1-base's sentence already names fm1-base and Felucca), and the fonts it uses
CREDITS = [(" Built on LOAF BREAKER by FLEXOR and CHOPPAH by Laurens Profittlich, on fm1-base and felucca-bsp, from Felucca by Leo Kuroshita (Hügelton Instruments).",
            " Built on LOAF BREAKER by FLEXOR and CHOPPAH by Laurens Profittlich."),
           ("Fonts: Barlow Semi Condensed (SIL OFL 1.1).",
            "Fonts: Elektron Pixel Font by savingaurora, adapted (CC BY-SA 4.0); Inter Tight (SIL OFL 1.1).")]


def main(src, dst, untested):
    html = open(src, encoding="utf-8").read()
    for mark in ("</head>", "<body>\n", '  <p data-t="warn">'):
        if html.count(mark) != 1:
            raise SystemExit(f"install_theme: {mark!r} not found once in {src}: fm1-base's installer changed")
    html = html.replace("</head>", STYLE + "</head>", 1).replace("<body>\n", "<body>\n" + BAR, 1)
    html = html.replace("<h1>Install AFFINE</h1>", "<h1>Install</h1>", 1)   # (the bar says AFFINE: one word, one line)
    for a, b in CREDITS:
        if html.count(a) != 1:
            raise SystemExit(f"install_theme: {a[:40]!r}... not found once: the credits changed")
        html = html.replace(a, b, 1)
    if untested:
        html = html.replace('  <p data-t="warn">', NOTE + '  <p data-t="warn">', 1)
    open(dst, "w", encoding="utf-8").write(html)


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        sys.exit(__doc__)
    main(args[0], args[1], "--untested" in sys.argv)
