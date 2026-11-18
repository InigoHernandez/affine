// SPDX-License-Identifier: MPL-2.0
// AFFINE's site, 04 Controls: the FM-1 drawn (panel.js), every control pressable, its job in one line and the FM-1's
// screen for it (the simulator's: tests/scenarios/sitemap.txt). A page button works as on the FM-1 (docs/plan.md,
// controls phase 2; app/ui/main.c DM_*): pressed from another control its first part, pressed again the next, round;
// a part's items are what OCT steps through. What works with the control lights dim beside it.
"use strict";
(() => {
  const A = window.affine, svg = document.getElementById("map-svg");
  if (!svg || !A || typeof affPanel !== "function") return;
  const $ = (id) => document.getElementById(id);
  const P = affPanel(svg);
  const k = (l) => P.knobs[l], b = (l) => P.buttonsByLabel[l];
  const BLACK = PANEL_BLACK_K.map((i) => P.keys[i]), WHITE = PANEL_WHITE_K.map((i) => P.keys[i]);
  const DRUM = ["SOUND", "SHAPE", "MORE", "MEMORY"];
  // each control: its name, a tag, one line; on (lit), with (dim); a page button its parts (n, shot, items, with)
  const C = [
    { n: "MASTER", on: [k("MASTER")], tag: "Volume", line: "The volume.", shot: "m-home-perform", cap: "HOME" },
    { n: "SELECT", on: [k("SELECT")], tag: "Every page", line: "The tempo, whatever the page.", shot: "m-tempo", cap: "FX › CHAIN, SELECT turned" },
    { n: "PRESETS", on: [k("PRESETS")], tag: "Every page", line: "The pattern. Playing, it changes at the loop's end.", shot: "m-pattern", cap: "FX › CHAIN, PRESETS turned" },
    { n: "ALGORITHM", on: [k("ALGORITHM")], tag: "Every page", line: "The break: the next one, or the one before.", shot: "m-break", cap: "FX › CHAIN, ALGORITHM turned" },
    { n: "OCT− OCT+", on: [b("OCT-"), b("OCT+")], with: [b("FX")], tag: "Items", line: "The open part's items, one by one; where there are none, the pitch. With FX held: the black keys' row.",
      shot: "m-oct", cap: "FX › PSX, OCT+: VERB" },
    { n: "K1–K4", on: [k("K1"), k("K2"), k("K3"), k("K4")], tag: "The item", line: "The four settings at the screen's foot. On HOME: FILTER, CRUSH, REDUX, VERB.", shot: "m-knob", cap: "HOME, K1 turned" },
    { n: "FX", tag: "Page", line: "Every effect: sixteen on the white keys, the chain of four, the chip.", hold: "Held alone: the FX fader, 0 or 100 %", with: [b("OCT-"), b("OCT+")], parts: [
      { n: "PLAY", items: ["AFFINE", "PUNCH"], with: WHITE },
      { n: "CHAIN", items: ["SLOT 1", "SLOT 2", "SLOT 3", "SLOT 4"] },
      { n: "SLOT", items: ["MAIN", "MORE", "MOD"] },
      { n: "PSX", items: ["PRESET", "VERB", "CODEC", "COLOR", "LOFI", "SKIP", "OUT"] },
      { n: "SENDS" } ] },
    { n: "SEL", on: [b("SEL")], with: [b("SAVE"), b("HOME"), b("SEQ"), b("PLAY")], tag: "With another", line: "Tap it: its card. With SAVE: undo. With HOME, then HOME: INIT. With SEQ: copy.",
      shot: "m-sel", cap: "HOME, SEL tapped" },
    { n: "ENV", tag: "Page", line: "The song: tracks, drops, builds.", parts: [
      { n: "TRACKS", items: ["The song's items"] }, { n: "PLAY" }, { n: "DROP", items: ["A white key: the drop into its pattern"], with: WHITE }, { n: "BUILD", items: ["BUILD", "TEMPO RISE"] } ] },
    { n: "LFO", tag: "Page", line: "The mix: levels, the master bus, its dynamics.", parts: [{ n: "LEVELS" }, { n: "MASTER" }, { n: "DYN" }] },
    { n: "EDIT", tag: "Page", line: "The sounds: the break, BOOM, the drums.", parts: [
      { n: "BREAK", pitch: true }, { n: "BOOM", items: ["BOOM", "SHAPE"] }, { n: "SNARE", items: DRUM }, { n: "HAT", items: DRUM }, { n: "CLAP", items: DRUM } ] },
    { n: "GLO", tag: "Page", line: "The FM-1 itself: MIDI, setup, the screen, reset.", parts: [
      { n: "MIDI" }, { n: "SETUP" }, { n: "SCREEN", items: ["EFFECTS", "FX SET", "VIEWS"] }, { n: "RESET" } ] },
    { n: "HOME", tag: "Page", line: "The instrument: the break, its floor, four gestures.", parts: [{ n: "PERFORM", pitch: true }, { n: "SETTINGS" }] },
    { n: "SAVE", on: [b("SAVE")], tag: "Store", line: "Writes it all to the FM-1. Best while stopped.", shot: "m-save", cap: "HOME, SAVE pressed" },
    { n: "ARP", tag: "Page", line: "The drums written for you: COMPLEXITY, ANCHOR, ROLL, FILL.", parts: [{ n: "GENERATOR" }] },
    { n: "SEQ", tag: "Page", line: "The pattern's steps, lane by lane.", hold: "Held: CLEAR Pn? SEQ again clears it", parts: [
      { n: "STEPS", items: ["The lanes"] }, { n: "DRUMS", items: ["The lanes"] }, { n: "SETTINGS" } ] },
    { n: "PLAY", on: [b("PLAY")], tag: "Transport", line: "Plays and stops the break.", shot: "m-home-perform", cap: "HOME, playing" },
    { n: "REC", on: [b("REC")], with: BLACK, tag: "Transport", line: "Records the slices and the black keys as they play. It clears nothing.", shot: "m-rec", cap: "HOME, REC on" },
    { n: "Black keys", on: BLACK, with: [b("FX"), b("OCT-"), b("OCT+")], tag: "11 keys", line: "Effects while held, in two rows: AFFINE, PUNCH.", shot: "k-roll16", cap: "HOME, ROLL 1/16 held", link: "#perform" },
    { n: "White keys", on: WHITE, tag: "16 keys", line: "The break's sixteen slices.", shot: "m-white", cap: "HOME, a slice held" },
  ];
  for (const c of C) if (c.parts) {
    c.on = [b(c.n)];
    c.parts.forEach((p) => { p.shot = `m-${c.n.toLowerCase()}-${p.n.toLowerCase()}`; });
  }
  const scr = $("map-scr"), lcd = $("map-lcd"), parts = $("map-parts"), items = $("map-items");
  let cur = -1, part = 0;
  const all = new Set(C.flatMap((c) => c.on));
  function lights() {
    const c = C[cur], p = c.parts?.[part];
    const on = new Set(c.on), dim = new Set([...(c.with || []), ...(p?.with || [])]);
    for (const x of all) {
      x.g.classList.toggle("lit", on.has(x));
      x.g.classList.toggle("dim", !on.has(x) && dim.has(x));
    }
  }
  function render(say) {
    const c = C[cur], p = c.parts?.[part];
    $("map-name").textContent = c.n;
    $("map-tag").textContent = c.tag;
    $("map-line").textContent = c.line;
    // the parts: a segmented row, the open one lit
    parts.hidden = !c.parts;
    if (c.parts) {
      if (parts.dataset.of !== c.n) {
        parts.dataset.of = c.n;
        parts.replaceChildren(...c.parts.map((q, i) => {
          const x = document.createElement("button");
          x.type = "button";
          x.textContent = q.n;
          x.addEventListener("click", () => { part = i; render(true); });
          return x;
        }));
        parts.style.setProperty("--n", c.parts.length);
      }
      parts.style.setProperty("--on", part);
      [...parts.children].forEach((x, i) => x.setAttribute("aria-pressed", i === part ? "true" : "false"));
    }
    // what OCT steps through on this part (or the pitch), and what a hold does
    items.replaceChildren();
    const its = p ? (p.pitch ? ["The pitch"] : p.items) : null;
    if (its) {
      items.append(Object.assign(document.createElement("span"), { className: "label map__k", textContent: "OCT" }));
      its.forEach((t, j) => items.append(Object.assign(document.createElement("span"), { className: "badge", textContent: t, style: `--i:${j}` })));
    }
    if (c.hold) {
      const h = document.createElement("span");
      h.className = "map__hold";
      h.append(Object.assign(document.createElement("span"), { className: "label map__k", textContent: "Hold" }), c.hold);
      items.append(h);
    }
    items.hidden = !items.childElementCount;
    $("map-hint").textContent = c.parts && c.parts.length > 1 ? `Press ${c.n} again: the next part` : "";
    $("map-go").hidden = !c.link;
    $("map-n").textContent = `${String(cur + 1).padStart(2, "0")} / ${C.length}`;
    const shot = p ? p.shot : c.shot;
    $("map-cap").textContent = p ? `${c.n} › ${p.n}` : c.cap;
    A.draw(scr, `screens/${shot}.png`);
    A.draw(lcd, `screens/${shot}.png`);
    lights();
    reach(c.on[0].g);
    if (say) $("map-say").textContent = `${c.n}${p ? `, ${p.n}` : ""}. ${c.line}`;
  }
  // a phone: the panel wider than the page, swiped across; the control chosen slides into view
  const box = svg.closest(".map__panel");
  function reach(g) {
    if (box.scrollWidth <= box.clientWidth + 1) return;
    const r = g.getBoundingClientRect(), o = box.getBoundingClientRect();
    if (r.left >= o.left + 8 && r.right <= o.right - 8) return;
    box.scrollTo({ left: box.scrollLeft + r.left + r.width / 2 - (o.left + o.width / 2), behavior: A.motion ? "smooth" : "auto" });
  }
  // pressed: another control opens at its first part; the same page button again, the next part, round
  function press(i, say = true) {
    if (i === cur && C[i].parts) part = (part + 1) % C[i].parts.length;
    else { cur = i; part = 0; }
    render(say);
  }
  C.forEach((c, i) => {
    c.on.forEach((x, j) => {
      x.g.addEventListener("click", () => {
        press(i);
        x.g.classList.remove("press"); void x.g.getBoundingClientRect(); x.g.classList.add("press");
        setTimeout(() => x.g.classList.remove("press"), 140);
      });
      if (j) { x.g.setAttribute("aria-hidden", "true"); return; }   // (one stop a control for the keyboard)
      x.g.setAttribute("tabindex", "0");
      x.g.setAttribute("role", "button");
      x.g.setAttribute("aria-label", c.n);
      x.g.addEventListener("keydown", (e) => { if (e.key === " " || e.key === "Enter") { e.preventDefault(); press(i); } });
    });
  });
  $("map-prev").addEventListener("click", () => { cur = (cur + C.length - 1) % C.length; part = 0; render(true); });
  $("map-next").addEventListener("click", () => { cur = (cur + 1) % C.length; part = 0; render(true); });
  cur = C.findIndex((c) => c.n === "HOME");
  render(false);
})();
